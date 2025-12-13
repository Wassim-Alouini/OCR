#include <stdio.h>
#include "image_loader.h"
#include "window_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_rotozoom.h>
#include <math.h>
#include <string.h>
#include "cmd_window.h"
#include "bounds.h"
#include "detectgrid.h"
#include <time.h>
#include "config.h"
#include "../NeuralNetwork/neuralnetwork.h"
#include "preprocess.h"
#include "../Solver/solver.h"

int ended = 0;

void rotate_and_render
(SDL_Renderer* renderer, double angle, SDL_Surface** master_surface, SDL_Window* window, SDL_Texture** window_output);
void apply_grayscale
(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture** window_output);
void blur_gaussian3(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture **window_output);
void binarize_otsu(SDL_Renderer* renderer,
                   SDL_Surface* master_surface,
                   SDL_Texture **window_output);
void close_black3x3(SDL_Renderer* renderer,
SDL_Surface* master_surface,
SDL_Texture **window_output,
int iterations);
void median3x3(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture **window_output);
double estimate_rotation_angle_projection_full(SDL_Surface **surface);
void repair_strokes(SDL_Renderer *renderer,
                    SDL_Surface *master_surface,
                    SDL_Texture **window_output);
void binarize(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture **window_output);
char *line_maker(int lineCount,const char* folder,NeuralNetwork* nn);
void grid_maker(Box*** gridLines,int **gridCount,
		int nbLinesGrid,SDL_Surface** master_surface, NeuralNetwork* nn);
char** word_maker(Box*** wordLines,int **wordCount,int nbLinesWord,SDL_Surface** master_surface,NeuralNetwork* nn);
void solver_call(SDL_Renderer* renderer,Box*** gridBoxes,char **wordlist, int nbLinesWord,int* x1, int* y1, int* x2,int* y2);
void solver_call_surface(SDL_Surface** surface,Box*** gridBoxes,char **wordlist, int nbLinesWord,int* x1, int* y1, int* x2,int* y2);
Coord (*solve(Box*** gridBoxes, char **wordlist, int nbLinesWord))[2];
void drawsolution(Coord (*to_draw)[2],
                  SDL_Surface** master_surface,
                  Box*** gridBoxes,
                  int *gridCount,
                  int nbLinesGrid,
                  int num);


static int load_into_master(SDL_Renderer *renderer,
                            SDL_Surface **master_surface,
                            SDL_Window *window,
                            SDL_Texture **window_output,
                            const char *filename)
{
    if (!renderer || !master_surface || !window || !window_output || !filename || !*filename) {
        fprintf(stderr, "load_into_master: invalid args\n");
        return 0;
    }

    // Convert anything -> BMP (so your existing load_image(SDL_LoadBMP) works)
    const char *tmpbmp = "output.bmp";  // reuse your existing path
    char syscmd[512];
    snprintf(syscmd, sizeof(syscmd), "magick \"%s\" \"%s\"", filename, tmpbmp);

    int res = system(syscmd);
    if (res != 0) {
        fprintf(stderr, "load: magick convert failed (%d) for '%s'\n", res, filename);
        return 0;
    }

    // Free old surface
    if (*master_surface) {
        SDL_FreeSurface(*master_surface);
        *master_surface = NULL;
    }

    // Load new surface
    load_image(master_surface, tmpbmp);
    if (!*master_surface) {
        fprintf(stderr, "load: load_image failed for '%s'\n", tmpbmp);
        return 0;
    }

    // Resize + center window to new surface size
    int neww = (*master_surface)->w;
    int newh = (*master_surface)->h;

    SDL_SetWindowSize(window, neww, newh);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Replace output texture
    if (*window_output) {
        SDL_DestroyTexture(*window_output);
        *window_output = NULL;
    }

    *window_output = SDL_CreateTextureFromSurface(renderer, *master_surface);
    if (!*window_output) {
        fprintf(stderr, "load: texture creation failed: %s\n", SDL_GetError());
        return 0;
    }

    // Render immediately
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, *window_output, NULL, NULL);
    SDL_RenderPresent(renderer);

    return 1;
}

void RotateSurface(SDL_Surface **masterSurface, double alpha)
{
    if (!masterSurface || !*masterSurface)
        return;

    SDL_Surface *src = *masterSurface;

    // Work in 32-bit RGBA to keep things simple
    SDL_Surface *work = src;
    int must_free_work = 0;

    if (src->format->BitsPerPixel != 32) {
        work = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0);
        if (!work) {
            fprintf(stderr, "RotateSurface: SDL_ConvertSurfaceFormat failed: %s\n",
                    SDL_GetError());
            return;
        }
        must_free_work = 1;
    }

    // angle in degrees, zoom = 1.0, smooth = 1
    SDL_Surface *rotated = rotozoomSurface(work, alpha, 1.0, 1);
    if (!rotated) {
        fprintf(stderr, "RotateSurface: rotozoomSurface failed: %s\n",
                SDL_GetError());
        if (must_free_work)
            SDL_FreeSurface(work);
        return;
    }

    if (must_free_work)
        SDL_FreeSurface(work);

    if (rotated->format->BitsPerPixel != 32) {
        SDL_Surface *converted =
            SDL_ConvertSurfaceFormat(rotated, SDL_PIXELFORMAT_RGBA32, 0);
        if (!converted) {
            fprintf(stderr, "RotateSurface: post-convert failed: %s\n",
                    SDL_GetError());
            SDL_FreeSurface(rotated);
            return;
        }
        SDL_FreeSurface(rotated);
        rotated = converted;
    }

    if (SDL_MUSTLOCK(rotated))
        SDL_LockSurface(rotated);

    Uint32 *pixels = (Uint32 *)rotated->pixels;
    SDL_PixelFormat *fmt = rotated->format;
    int total = rotated->w * rotated->h;

    for (int i = 0; i < total; ++i) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[i], fmt, &r, &g, &b, &a);

        if (a == 0) {
            pixels[i] = SDL_MapRGBA(fmt, 255, 255, 255, 255);  // white
        }
    }

    if (SDL_MUSTLOCK(rotated))
        SDL_UnlockSurface(rotated);

    SDL_FreeSurface(src);
    *masterSurface = rotated;
}

//Given bounding boxes of elements (letters, words, grid, list of words)
//Create BMP files containing the pixels bound by each box.
void extract_boxes_to_bmp
(SDL_Surface* master_surface, Box* boxes, int box_count, const char* folder)
{
    if (!master_surface || !boxes || box_count <= 0) 
    {
        fprintf(stderr, "extract_boxes_to_bmp: invalid input\n");
        return;
    }

    for (int i = 0; i < box_count; i++) 
    {
        Box b = boxes[i];
    
//      b.x -= 1;
//      b.y -= 1;
//      b.w += 2;
//      b.h += 2;

        if (b.x < 0) b.x = 0;
        if (b.y < 0) b.y = 0;
        if (b.x + b.w > master_surface->w) b.w = master_surface->w - b.x;
        if (b.y + b.h > master_surface->h) b.h = master_surface->h - b.y;
        if (b.w <= 0 || b.h <= 0) continue;

        SDL_Rect rect = { b.x, b.y, b.w, b.h };
        SDL_Surface* sub = SDL_CreateRGBSurfaceWithFormat(0, b.w, b.h,
            master_surface->format->BitsPerPixel,
            master_surface->format->format);
        if (!sub) 
        {
            continue;
        }
        if (SDL_BlitSurface(master_surface, &rect, sub, NULL) < 0) 
        {
            SDL_FreeSurface(sub);
            continue;
        }
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/_box_%d.bmp", folder ? folder : "images", i);
        //printf("Saved %s\n", filename);

        if (folder) 
        {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p %s", folder);
            system(cmd);
        }

        if (SDL_SaveBMP(sub, filename) != 0) 
        {
            fprintf(stderr, "Erreur lors de la sauvegarde de %s: %s\n", filename, SDL_GetError());
        } 
        else 
        {
        //printf("Saved %s\n", filename);
        }
    
        SDL_FreeSurface(sub);
    }
}

//Copies the rendered rotated texture into the master_surface. Here master_surface must
//be rotated so be able to apply the operations to it.
void apply_rotation_to_surface
    (SDL_Renderer* renderer, SDL_Texture* window_texture,
    SDL_Surface** master_surface)
{
    if (!renderer || !window_texture || !master_surface || !(*master_surface)) 
    {
        SDL_Log("apply_rotation_to_surface: Invalid parameters");
        return;
    }
    int tex_w, tex_h;
    SDL_QueryTexture(window_texture, NULL, NULL, &tex_w, &tex_h);
    SDL_Surface* rotated_surface = SDL_CreateRGBSurfaceWithFormat(0, tex_w, tex_h,
        (*master_surface)->format->BitsPerPixel, (*master_surface)->format->format);
    if (!rotated_surface) 
    {
        SDL_Log("apply_rotation_to_surface: Failed to create surface: %s", SDL_GetError());
        return;
    }
    SDL_Texture* target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, window_texture);
    if (SDL_RenderReadPixels(renderer, NULL,
        rotated_surface->format->format,
        rotated_surface->pixels,
        rotated_surface->pitch) != 0)
    {
        SDL_Log("apply_rotation_to_surface: SDL_RenderReadPixels failed: %s", SDL_GetError());
        SDL_FreeSurface(rotated_surface);
        SDL_SetRenderTarget(renderer, target);
        return;
    }

    SDL_SetRenderTarget(renderer, target);

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(rotated_surface, (*master_surface)->format->format, 0);
    SDL_FreeSurface(rotated_surface);

    SDL_FreeSurface(*master_surface);
    *master_surface = converted;
}

//Event handler manages quitting, as well as terminal inputs "rotate <angle>"
//"grayscale" "binarize <threshold>" "boxes"
int event_handler(SDL_Renderer* renderer,
                  SDL_Surface** master_surface,
                  SDL_Window* window,
                  SDL_Texture** window_output)
{
    SDL_Event current;

    while (SDL_PollEvent(&current))
    {
        switch (current.type)
        {
            case SDL_QUIT:
                return 0;
            default:
                break;
        }

        // NEW: struct result (operation + raw + value)
        CmdResult cmd = cmdwindow_handle_event(&current);

        // If nothing entered, skip
        if (cmd.operation[0] == '\0')
            continue;

        if (strcmp(cmd.operation, "rotate") == 0)
        {
            rotate_and_render(renderer, cmd.value, master_surface, window, window_output);
        }

        if (strcmp(cmd.operation, "autorotate") == 0)
        {
            double alpha = estimate_rotation_angle_projection_full(master_surface);
            printf("autorotate(full): %.3f deg\n", alpha);
            rotate_and_render(renderer, alpha, master_surface, window, window_output);
        }

        if (strcmp(cmd.operation, "grayscale") == 0)
        {
            apply_grayscale(renderer, *master_surface, window_output);
        }

        if (strcmp(cmd.operation, "binarize") == 0)
        {
            binarize(renderer, *master_surface, window_output);
        }

        if (strcmp(cmd.operation, "boxes") == 0)
        {
            ended = 1;

            int blob_count = 0;
            int* blob_sizes = NULL;
            Coord** blobs = find_blobs_rec(master_surface, &blob_count, &blob_sizes);
            printf("Found %d blobs\n", blob_count);

            Box *boxes = compute_blob_boxes(blobs, blob_sizes, blob_count);
            int newcount;
            Box* myboxes = extract(boxes, blob_count, &newcount);

            int testCount;
            Box* testBox = Find_Letters(myboxes, newcount, &testCount);

            Box*** gridBoxes = malloc(sizeof(Box**));
            Box*** wordBoxes = malloc(sizeof(Box**));
            int** gridCount = malloc(sizeof(int*));
            int** wordCount = malloc(sizeof(int*));
            int nbLinesGrid = 0;
            int nbLinesWord = 0;

            separate_grid_word(testBox, testCount,
                               gridBoxes, gridCount,
                               wordBoxes, wordCount,
                               &nbLinesGrid, &nbLinesWord);

            printf("nblettres: %i\n", testCount);
            printf("nbword: %i\n", nbLinesWord);

            for (int i = 0; i < nbLinesGrid; i++)
                draw_boxes(master_surface, (*gridBoxes)[i], (*gridCount)[i], 0, 0, 255);

            for (int i = 0; i < nbLinesWord; i++)
                draw_boxes(master_surface, (*wordBoxes)[i], (*wordCount)[i], 255, 0, 0);

            if (*window_output)
                SDL_DestroyTexture(*window_output);
            *window_output = SDL_CreateTextureFromSurface(renderer, *master_surface);

            extract_boxes_to_bmp(*master_surface, testBox, testCount, "images");
        }

        // NEW: load uses cmd.raw so you can pass a filename
        // command line: "load filename.png"
        if (strcmp(cmd.operation, "load") == 0)
        {
            const char *p = cmd.raw + 4;     // skip "load"
            while (*p == ' ') p++;

            if (*p == '\0') {
                printf("usage: load <filename>\n");
            } else {
                printf("loading: %s\n", p);
                load_into_master(renderer, master_surface, window, window_output, p);
            }
        }

        if (strcmp(cmd.operation, "blur") == 0)
        {
            blur_gaussian3(renderer, *master_surface, window_output);
        }

        if (strcmp(cmd.operation, "close") == 0)
        {
            close_black3x3(renderer, *master_surface, window_output, 1);
        }

        if (strcmp(cmd.operation, "median") == 0)
        {
            median3x3(renderer, *master_surface, window_output);
        }

        if (strcmp(cmd.operation, "repair") == 0)
        {
            repair_strokes(renderer, *master_surface, window_output);
        }

        if (strcmp(cmd.operation, "auto") == 0)
        {
            apply_grayscale(renderer, *master_surface, window_output);
            median3x3(renderer, *master_surface, window_output);
            binarize(renderer, *master_surface, window_output);

            double alpha = estimate_rotation_angle_projection_full(master_surface);
            printf("autorotate(full): %.3f deg\n", alpha);
            rotate_and_render(renderer, alpha, master_surface, window, window_output);

            binarize(renderer, *master_surface, window_output);

            ended = 1;
            int blob_count = 0;
            int* blob_sizes = NULL;
            Coord** blobs = find_blobs_rec(master_surface, &blob_count, &blob_sizes);
            printf("Found %d blobs\n", blob_count);

            Box *boxes = compute_blob_boxes(blobs, blob_sizes, blob_count);
            int newcount;
            Box* myboxes = extract(boxes, blob_count, &newcount);

            int testCount;
            Box* testBox = Find_Letters(myboxes, newcount, &testCount);

            Box*** gridBoxes = malloc(sizeof(Box**));
            Box*** wordBoxes = malloc(sizeof(Box**));
            int** gridCount = malloc(sizeof(int*));
            int** wordCount = malloc(sizeof(int*));
            int nbLinesGrid = 0;
            int nbLinesWord = 0;

            separate_grid_word(testBox, testCount,
                               gridBoxes, gridCount,
                               wordBoxes, wordCount,
                               &nbLinesGrid, &nbLinesWord);

            printf("nblettres: %i\n", testCount);
            printf("nbword: %i\n", nbLinesWord);

            int x1 = 0;
            int y1 = 0;
            int x2 = 0;
            int y2 = 0;

            printf("before nn");

            NeuralNetwork* nn = nn_load("Graphics/model/model_final.txt");
            grid_maker(gridBoxes,gridCount,nbLinesGrid,master_surface,nn);
            char** wordlist = word_maker(wordBoxes,wordCount,nbLinesWord,master_surface,nn);
            Coord (*first)[2] = solve(gridBoxes, wordlist, nbLinesWord);
            if (!first) { printf("solve failed (first)\n"); break; }

            if(remove("grid.txt") != 0)
            printf("remove\n");

            printf("before nn2");

            NeuralNetwork* nn2 = nn_load("Graphics/model/model_finetune.txt");
            grid_maker(gridBoxes,gridCount,nbLinesGrid,master_surface,nn2);
            char** wordlist2 = word_maker(wordBoxes,wordCount,nbLinesWord,master_surface,nn2);
            Coord (*second)[2] = solve(gridBoxes, wordlist2, nbLinesWord);
            if (!second) { printf("solve failed (second)\n"); free(first); break; }
            printf("after solve2\n");

            printf("before sizes");

            int firstsize = 0;
            int secondsize = 0;

            for(int i = 0; i < nbLinesWord; i++)
            {
                if(first[i][0].x == 0 && first[i][0].y == 0 && first[i][1].x == 0 && first[i][1].y == 0)
                {
                    continue;
                }
                firstsize++;
            }

            for(int i = 0; i < nbLinesWord; i++)
            {
                if(second[i][0].x == 0 && second[i][0].y == 0 && second[i][1].x == 0 && second[i][1].y == 0)
                {
                    continue;
                }
                secondsize++;
            }

            Coord firstfix[firstsize][2];
            Coord secondfix[secondsize][2];

            int count = 0;
            for(int i = 0; i < nbLinesWord; i++)
            {
                if(first[i][0].x == 0 && first[i][0].y == 0 && first[i][1].x == 0 && first[i][1].y == 0)
                {
                    continue;
                }
                firstfix[count][0] = first[i][0];
                firstfix[count][1] = first[i][1];
                count++;
            }

            int count2 = 0;
            for(int i = 0; i < nbLinesWord; i++)
            {
                if(second[i][0].x == 0 && second[i][0].y == 0 && second[i][1].x == 0 && second[i][1].y == 0)
                {
                    continue;
                }
                secondfix[count2][0] = second[i][0];
                secondfix[count2][1] = second[i][1];
                count2++;
            }

            printf("count1 = %i", count);
            printf("count2 = %i", count2);

            if(firstsize >= secondsize)
            {
                drawsolution(firstfix, master_surface, gridBoxes, *gridCount, nbLinesGrid, firstsize);

            }
            else
                drawsolution(secondfix, master_surface, gridBoxes, *gridCount, nbLinesGrid, secondsize);


            //draw_word(renderer,gridBoxes,y1,x1,y2,x2);

            if(remove("grid.txt") != 0)
            printf("remove\n");

            free(first);
            free(second);

            if (*window_output)
                SDL_DestroyTexture(*window_output);
            *window_output = SDL_CreateTextureFromSurface(renderer, *master_surface);

            //extract_boxes_to_bmp(*master_surface, testBox, testCount, "images");
        }
        if (strcmp(cmd.operation, "learn") == 0) 
        {
            char dataset[256], out[256];
            int epochs = 10, hidden = 64;
            double lr = 0.01;

            if (sscanf(cmd.raw, "learn %255s %255s %d %lf %d",
                    dataset, out, &epochs, &lr, &hidden) == 5)
            {
                train(dataset, out, epochs, lr, hidden,
                cmdwindow_get_logger(), cmdwindow_get_logger_userdata());
            }
            else
            {
                printf("usage: learn <dataset> <model_out> <epochs> <lr> <hidden>\n");
            }
        }

    }




    return 1;
}

char *line_maker(int lineCount,const char* folder,NeuralNetwork* nn)
{
	char* res = malloc((lineCount + 1) * sizeof(char));
	if(!res)
	{
		fprintf(stderr, "line_maker: Error malloc\n");
		return NULL;
	}

	for(int i = 0; i < lineCount; i++)
	{
		char filename[256];
        snprintf(filename, sizeof(filename), "%s/_box_%d.bmp", folder, i);
		char letter = nn_predict_from_model(nn,filename);
		res[i] = letter;
	}

	res[lineCount] = 0;
    printf("%s\n",res);
	return res;
}

void grid_maker(Box*** gridLines,int **gridCount,
		int nbLinesGrid,SDL_Surface** master_surface, NeuralNetwork* nn)
{
	for(int i = 0; i < nbLinesGrid; i++)
	{
		extract_boxes_to_bmp(*master_surface,(*gridLines)[i],(*gridCount)[i], "images");
		char* line = line_maker((*gridCount)[i],"images",nn);
		if(!line)
		{
			return;
		}

                char filename[256];

                for (int j = 0; j < (*gridCount)[i]; j++)
                {
                        snprintf(filename,sizeof(filename),"%s/_box_%d.bmp","images",j);
                        if (remove(filename) != 0)
                        {
                                return;
                        }
                }

		FILE *f = fopen("grid.txt", "a");
    		if (!f)
		{
			free(line);	
        		return;
		}

    		fprintf(f, "%s\n",line);

    		fclose(f);
		free(line);
	}
	
}

char** word_maker(Box*** wordLines,int **wordCount,int nbLinesWord,SDL_Surface** master_surface,NeuralNetwork* nn)
{
	char ** wordList = malloc(nbLinesWord * sizeof(char*));
	if(!wordList)
		return NULL;

        for(int i = 0; i < nbLinesWord; i++)
        {
                extract_boxes_to_bmp(*master_surface,(*wordLines)[i],(*wordCount)[i], "images");
                char* line = line_maker((*wordCount)[i],"images",nn);
                if(!line)
		{
			for (int k = 0; k < i; k++)
                	{
				free(wordList[k]);
			}

		    	free(wordList);
            		return NULL;
		}
		
		char filename[256];

    		for (int j = 0; j < (*wordCount)[i]; j++)
    		{
		        snprintf(filename,sizeof(filename),"%s/_box_%d.bmp","images",j);
	
        		if (remove(filename) != 0)
        		{
				for (int k = 0; k < i; k++)
                        	{
                                	free(wordList[k]);
                        	}

                        	free(wordList);
                        	return NULL;
        		}
    		}

        	wordList[i] = line;        
        }


	return wordList;
}

void solver_call(SDL_Renderer* renderer,Box*** gridBoxes,char **wordlist, int nbLinesWord,int* x1, int* y1, int* x2,int* y2)
{
	for(int i = 0; i < nbLinesWord; i++)
	{
		solver("grid.txt",wordlist[i],x1,y1,x2,y2);
        srand(time(NULL));
        draw_word(renderer,gridBoxes,*y1,*x1,*y2,*x2);
	}
}

void solver_call_surface(SDL_Surface** surface,Box*** gridBoxes,char **wordlist, int nbLinesWord,int* x1, int* y1, int* x2,int* y2)
{
	for(int i = 0; i < nbLinesWord; i++)
	{
		solver("grid.txt",wordlist[i],x1,y1,x2,y2);

        srand(time(NULL));
        draw_word_surface(surface,gridBoxes,*y1,*x1,*y2,*x2);
	}
}


Coord (*solve(Box*** gridBoxes, char **wordlist, int nbLinesWord))[2]
{
    (void)gridBoxes; // not needed here, keep signature for compatibility

    Coord (*res)[2] = malloc((size_t)nbLinesWord * sizeof(*res));
    if (!res) {
        fprintf(stderr, "solve: malloc failed\n");
        return NULL;
    }

    for (int i = 0; i < nbLinesWord; i++) {
        res[i][0].x = 0; res[i][0].y = 0;
        res[i][1].x = 0; res[i][1].y = 0;

        // solver expects int* for coords
        solver("grid.txt", wordlist[i],
               &res[i][0].x, &res[i][0].y,
               &res[i][1].x, &res[i][1].y);
    }

    return res; // caller must free(res)
}


void drawsolution(Coord (*to_draw)[2],
                  SDL_Surface** master_surface,
                  Box*** gridBoxes,
                  int *gridCount,
                  int nbLinesGrid,
                  int num)
{
    for (int i = 0; i < num; i++) {
        int x1 = to_draw[i][0].x;
        int y1 = to_draw[i][0].y;
        int x2 = to_draw[i][1].x;
        int y2 = to_draw[i][1].y;

        // reject "no solution"
        if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0)
            continue;

        // bounds check rows
        if (y1 < 0 || y1 >= nbLinesGrid || y2 < 0 || y2 >= nbLinesGrid) {
            printf("[draw] skip %d: row OOB (%d,%d)\n", i, y1, y2);
            continue;
        }

        // bounds check cols using gridCount per row
        int cols1 = gridCount[y1];
        int cols2 = gridCount[y2];

        if (x1 < 0 || x1 >= cols1 || x2 < 0 || x2 >= cols2) {
            printf("[draw] skip %d: col OOB (%d/%d, %d/%d)\n", i, x1, cols1, x2, cols2);
            continue;
        }

        draw_word_surface(master_surface, gridBoxes, y1, x1, y2, x2);
    }
}



//Update loop, renders the texture while running.
void update(SDL_Renderer* renderer, SDL_Surface** master_surface, SDL_Window* window, SDL_Texture** texture)
{
    int running = 1;
    while (running)
    {
        running = event_handler(renderer, master_surface, window, texture);

        SDL_RenderCopy(renderer, *texture, NULL, NULL);
        SDL_RenderPresent(renderer);

    }
}



//Main initializes elements, calls update, and terminates elements after Quit
int main(void)
{
    TTF_Init();

    SDL_Window *window = NULL;
    initialize_window(&window);

    cmdwindow_init();

    SDL_Renderer *renderer = NULL;
    initialize_renderer(&renderer, window);

    int start_w = 800;
    int start_h = 600;

    SDL_Surface *master_surface =
        SDL_CreateRGBSurfaceWithFormat(
            0, start_w, start_h, 32, SDL_PIXELFORMAT_RGBA32);

    if (!master_surface)
        errx(EXIT_FAILURE, "Failed to create master surface");

    SDL_FillRect(master_surface, NULL,
                 SDL_MapRGB(master_surface->format, 255,255,255));

    SDL_SetWindowSize(window, start_w, start_h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED);

    SDL_Texture *window_texture = NULL;
    initialize_window_texture(&window_texture, renderer, start_w, start_h);

    SDL_Texture *image_texture = NULL;
    create_texture_from_surface(&image_texture, renderer, master_surface);
    SDL_RenderCopy(renderer, image_texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    update(renderer, &master_surface, window, &window_texture);

    terminate_texture(image_texture);
    terminate_texture(window_texture);
    SDL_FreeSurface(master_surface);
    terminate_renderer(renderer);
    terminate_window(window);

    SDL_Quit();
    return EXIT_SUCCESS;
}

//Rotates and renders a texture.
void rotate_and_render(SDL_Renderer* renderer,
                       double angle,
                       SDL_Surface** master_surface,   // NOTE: double pointer
                       SDL_Window* window,
                       SDL_Texture** window_output)
{
    if (!renderer || !master_surface || !*master_surface || !window) {
        return;
    }

    // 1) Rotate the surface itself (this may change the pointer and its w/h)
    RotateSurface(master_surface, angle);

    SDL_Surface* rotated_surface = *master_surface;
    int maxw = rotated_surface->w;
    int maxh = rotated_surface->h;

    // 2) Resize and reposition the window to fit the new rotated surface
    SDL_SetWindowSize(window, maxw, maxh);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // 3) Recreate output texture for the window
    if (*window_output) {
        SDL_DestroyTexture(*window_output);
        *window_output = NULL;
    }

    SDL_Texture* window_texture = NULL;
    initialize_window_texture(&window_texture, renderer, maxw, maxh);

    SDL_Texture* image_texture = NULL;
    create_texture_from_surface(&image_texture, renderer, rotated_surface);

    // 4) Render the already-rotated texture into the window texture (no extra rotation)
    // If you want to use your existing helper, just call it with angle = 0:
    //
    // render_texture_rotated(renderer,
    //                        image_texture,
    //                        window_texture,
    //                        maxw, maxh,        // size of target
    //                        maxw, maxh,        // size of source (rotated_surface)
    //                        0.0);              // no extra rotation

    // Or do it directly with SDL_RenderCopy:
    SDL_SetRenderTarget(renderer, window_texture);
    SDL_RenderClear(renderer);

    SDL_Rect dst = { 0, 0, maxw, maxh };
    SDL_RenderCopy(renderer, image_texture, NULL, &dst);

    SDL_SetRenderTarget(renderer, NULL);

    *window_output = window_texture;

    terminate_texture(image_texture);
}

//Applies grayscale filter to surface
void apply_grayscale(SDL_Renderer* renderer,
                     SDL_Surface* master_surface,
                     SDL_Texture** window_output)
{
    if (!renderer || !master_surface || !window_output) 
    {
        fprintf(stderr, "apply_grayscale: invalid argument\n");
        return;
    }
    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint8 r, g, b;
    Uint32 *pixels = (Uint32 *)master_surface->pixels;
    SDL_PixelFormat *fmt = master_surface->format;
    int total_pixels = master_surface->w * master_surface->h;

    for (int i = 0; i < total_pixels; i++) 
    {
        SDL_GetRGB(pixels[i], fmt, &r, &g, &b);
        Uint8 gray = (Uint8)(0.299 * r + 0.587 * g + 0.114 * b);
        pixels[i] = SDL_MapRGB(fmt, gray, gray, gray);
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_UnlockSurface(master_surface);
    if (*window_output)
        SDL_DestroyTexture(*window_output);
    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "apply_grayscale: texture creation failed: %s\n", SDL_GetError());
}

//Given a threshold, binarizes surface, such that if pixel is above threshold it is made white, otherwise black


static inline int is_foreground(SDL_Surface *s, Uint32 px, int foreground_is_black)
{
    Uint8 r,g,b,a;
    SDL_GetRGBA(px, s->format, &r,&g,&b,&a);
    if (a == 0) return 0;
    Uint8 gray = (Uint8)(0.299*r + 0.587*g + 0.114*b);
    return foreground_is_black ? (gray < 128) : (gray > 128);
}

static int detect_foreground_is_black(SDL_Surface *s, int step)
{
    // Count dark vs bright pixels; assume background dominates.
    long long dark = 0, bright = 0;

    if (SDL_MUSTLOCK(s)) SDL_LockSurface(s);

    Uint32 *pix = (Uint32*)s->pixels;
    int pitch32 = s->pitch / 4;

    for (int y = 0; y < s->h; y += step) {
        Uint32 *row = pix + y * pitch32;
        for (int x = 0; x < s->w; x += step) {
            Uint8 r,g,b,a;
            SDL_GetRGBA(row[x], s->format, &r,&g,&b,&a);
            if (a == 0) continue;
            Uint8 gray = (Uint8)(0.299*r + 0.587*g + 0.114*b);
            if (gray < 128) dark++;
            else bright++;
        }
    }

    if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);

    // If bright pixels are majority (typical white page), foreground is black.
    return (bright >= dark) ? 1 : 0;
}

// Projection "spikiness" for angle (radians). Higher = straighter.
static double projection_score_fg(SDL_Surface *s, double a_rad, int step, int fg_black)
{
    const int w = s->w, h = s->h;
    const int pitch32 = s->pitch / 4;
    const double ca = cos(a_rad), sa = sin(a_rad);

    const int diag = (int)ceil(sqrt((double)w*w + (double)h*h)) + 4;
    const int off = diag / 2;

    int *hx = (int*)calloc((size_t)diag, sizeof(int));
    int *hy = (int*)calloc((size_t)diag, sizeof(int));
    if (!hx || !hy) { free(hx); free(hy); return -1.0; }

    if (SDL_MUSTLOCK(s)) SDL_LockSurface(s);
    Uint32 *pix = (Uint32*)s->pixels;

    for (int y = 0; y < h; y += step) {
        Uint32 *row = pix + y * pitch32;
        for (int x = 0; x < w; x += step) {
            if (!is_foreground(s, row[x], fg_black)) continue;

            // rotate by -a: x' = x*cos + y*sin, y' = -x*sin + y*cos
            double xr =  x * ca + y * sa;
            double yr = -x * sa + y * ca;

            int ix = (int)lround(xr) + off;
            int iy = (int)lround(yr) + off;
            if ((unsigned)ix < (unsigned)diag) hx[ix]++;
            if ((unsigned)iy < (unsigned)diag) hy[iy]++;
        }
    }

    if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);

    double sx = 0.0, sy = 0.0;
    for (int i = 0; i < diag; i++) {
        double vx = (double)hx[i];
        double vy = (double)hy[i];
        sx += vx * vx;
        sy += vy * vy;
    }

    free(hx); free(hy);
    return sx + sy;
}

// Returns angle in degrees for rotate_and_render / RotateSurface.
double estimate_rotation_angle_projection_full(SDL_Surface **surface)
{
    if (!surface || !*surface) return 0.0;
    SDL_Surface *s = *surface;

    if (s->format->BitsPerPixel != 32) {
        SDL_Surface *conv = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
        if (!conv) return 0.0;
        SDL_FreeSurface(s);
        *surface = s = conv;
    }

    const int STEP_PIXELS = 2;        // sampling stride
    const double COARSE_STEP = 0.5;   // degrees
    const double FINE_STEP   = 0.05;  // degrees
    const double FINE_WIN    = 1.0;   // degrees

    int fg_black = detect_foreground_is_black(s, 4);

    // 1) coarse search over 0..180
    double best_deg = 0.0;
    double best_score = -1.0;

    for (double deg = 0.0; deg <= 180.0 + 1e-9; deg += COARSE_STEP) {
        double sc = projection_score_fg(s, deg * M_PI / 180.0, STEP_PIXELS, fg_black);
        if (sc > best_score) { best_score = sc; best_deg = deg; }
    }

    // 2) refine around best
    double lo = best_deg - FINE_WIN;
    double hi = best_deg + FINE_WIN;
    if (lo < 0.0) lo = 0.0;
    if (hi > 180.0) hi = 180.0;

    double best2_deg = best_deg;
    double best2_score = -1.0;

    for (double deg = lo; deg <= hi + 1e-12; deg += FINE_STEP) {
        double sc = projection_score_fg(s, deg * M_PI / 180.0, STEP_PIXELS, fg_black);
        if (sc > best2_score) { best2_score = sc; best2_deg = deg; }
    }

    // Fold to [-45, +45] to prefer normal reading orientation (avoid +/-90° solution)
    double a = best2_deg;

    // normalize to [-180, 180]
    while (a > 180.0) a -= 360.0;
    while (a < -180.0) a += 360.0;

    // now fold modulo 90 into [-45, 45]
    while (a >  45.0) a -= 90.0;
    while (a < -45.0) a += 90.0;

    // If direction feels reversed in your SDL setup, flip once:
    // a = -a;

    return a;
}

static Uint8 otsu_threshold_from_surface(SDL_Surface *s)
{
    // Build grayscale histogram (0..255)
    unsigned int hist[256] = {0};

    if (SDL_MUSTLOCK(s)) SDL_LockSurface(s);

    Uint32 *pixels = (Uint32 *)s->pixels;
    SDL_PixelFormat *fmt = s->format;

    const int pitch32 = s->pitch / 4;
    const int w = s->w, h = s->h;

    for (int y = 0; y < h; y++) {
        Uint32 *row = pixels + y * pitch32;
        for (int x = 0; x < w; x++) {
            Uint8 r,g,b;
            SDL_GetRGB(row[x], fmt, &r, &g, &b);
            // luminance (same weights as your grayscale)
            Uint8 gray = (Uint8)(0.299 * r + 0.587 * g + 0.114 * b);
            hist[gray]++;
        }
    }

    if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);

    // Total pixels
    const double total = (double)w * (double)h;

    // Compute total mean level
    double sum_all = 0.0;
    for (int t = 0; t < 256; t++) sum_all += (double)t * (double)hist[t];

    // Otsu: maximize between-class variance
    double sum_b = 0.0;
    double w_b = 0.0;
    double max_var = -1.0;
    int best_t = 127;

    for (int t = 0; t < 256; t++) {
        w_b += (double)hist[t];
        if (w_b == 0.0) continue;

        double w_f = total - w_b;
        if (w_f == 0.0) break;

        sum_b += (double)t * (double)hist[t];

        double m_b = sum_b / w_b;
        double m_f = (sum_all - sum_b) / w_f;

        double var_between = w_b * w_f * (m_b - m_f) * (m_b - m_f);
        if (var_between > max_var) {
            max_var = var_between;
            best_t = t;
        }
    }

    return (Uint8)best_t;
}

// Otsu binarization (no threshold input)
void binarize_otsu(SDL_Renderer* renderer,
                   SDL_Surface* master_surface,
                   SDL_Texture **window_output)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "binarize_otsu: invalid argument\n");
        return;
    }

    Uint8 t = otsu_threshold_from_surface(master_surface);
    int ti = (int)t - 10;          // tweak: 5..20 depending on scans
    if (ti < 0) ti = 0;
    t = (Uint8)ti;
    // printf("Otsu threshold = %u\n", (unsigned)t);

    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint32 *pixels = (Uint32 *)master_surface->pixels;
    SDL_PixelFormat* fmt = master_surface->format;

    const int pitch32 = master_surface->pitch / 4;
    const int w = master_surface->w, h = master_surface->h;

    for (int y = 0; y < h; y++) {
        Uint32 *row = pixels + y * pitch32;
        for (int x = 0; x < w; x++) {
            Uint8 r, g, b;
            SDL_GetRGB(row[x], fmt, &r, &g, &b);
            Uint8 gray = (Uint8)(0.299 * r + 0.587 * g + 0.114 * b);
            Uint8 bw = (gray < t) ? 0 : 255;
            row[x] = SDL_MapRGB(fmt, bw, bw, bw);
        }
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_UnlockSurface(master_surface);

    if (*window_output)
        SDL_DestroyTexture(*window_output);

    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "binarize_otsu: texture creation failed: %s\n", SDL_GetError());
}
void blur_gaussian3(SDL_Renderer* renderer,
                    SDL_Surface* master_surface,
                    SDL_Texture **window_output)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "blur_gaussian3: invalid argument\n");
        return;
    }

    // Works best on 32bpp (RGBA32) which you now enforce at load.
    if (master_surface->format->BitsPerPixel != 32) {
        fprintf(stderr, "blur_gaussian3: surface must be 32bpp (got %d)\n",
                master_surface->format->BitsPerPixel);
        return;
    }

    const int w = master_surface->w;
    const int h = master_surface->h;
    const int pitch32 = master_surface->pitch / 4;

    // Temp buffer for a full copy of pixels
    Uint32 *src = (Uint32*)malloc((size_t)pitch32 * (size_t)h * sizeof(Uint32));
    if (!src) {
        fprintf(stderr, "blur_gaussian3: malloc failed\n");
        return;
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint32 *dst = (Uint32*)master_surface->pixels;
    memcpy(src, dst, (size_t)pitch32 * (size_t)h * sizeof(Uint32));

    SDL_PixelFormat *fmt = master_surface->format;

    // helper macro to clamp coords
    #define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            int x0 = CLAMP(x - 1, 0, w - 1);
            int x1 = x;
            int x2 = CLAMP(x + 1, 0, w - 1);

            int y0 = CLAMP(y - 1, 0, h - 1);
            int y1 = y;
            int y2 = CLAMP(y + 1, 0, h - 1);

            Uint8 r,g,b,a;

            // Accumulators
            int ar = 0, ag = 0, ab = 0, aa = 0;

            // Row y0
            SDL_GetRGBA(src[y0*pitch32 + x0], fmt, &r,&g,&b,&a); ar += 1*r; ag += 1*g; ab += 1*b; aa += 1*a;
            SDL_GetRGBA(src[y0*pitch32 + x1], fmt, &r,&g,&b,&a); ar += 2*r; ag += 2*g; ab += 2*b; aa += 2*a;
            SDL_GetRGBA(src[y0*pitch32 + x2], fmt, &r,&g,&b,&a); ar += 1*r; ag += 1*g; ab += 1*b; aa += 1*a;

            // Row y1
            SDL_GetRGBA(src[y1*pitch32 + x0], fmt, &r,&g,&b,&a); ar += 2*r; ag += 2*g; ab += 2*b; aa += 2*a;
            SDL_GetRGBA(src[y1*pitch32 + x1], fmt, &r,&g,&b,&a); ar += 4*r; ag += 4*g; ab += 4*b; aa += 4*a;
            SDL_GetRGBA(src[y1*pitch32 + x2], fmt, &r,&g,&b,&a); ar += 2*r; ag += 2*g; ab += 2*b; aa += 2*a;

            // Row y2
            SDL_GetRGBA(src[y2*pitch32 + x0], fmt, &r,&g,&b,&a); ar += 1*r; ag += 1*g; ab += 1*b; aa += 1*a;
            SDL_GetRGBA(src[y2*pitch32 + x1], fmt, &r,&g,&b,&a); ar += 2*r; ag += 2*g; ab += 2*b; aa += 2*a;
            SDL_GetRGBA(src[y2*pitch32 + x2], fmt, &r,&g,&b,&a); ar += 1*r; ag += 1*g; ab += 1*b; aa += 1*a;

            // Divide by 16 (kernel sum)
            Uint8 nr = (Uint8)(ar >> 4);
            Uint8 ng = (Uint8)(ag >> 4);
            Uint8 nb = (Uint8)(ab >> 4);
            Uint8 na = (Uint8)(aa >> 4);

            dst[y*pitch32 + x] = SDL_MapRGBA(fmt, nr, ng, nb, na);
        }
    }

    #undef CLAMP

    if (SDL_MUSTLOCK(master_surface))
        SDL_UnlockSurface(master_surface);

    free(src);

    if (*window_output)
        SDL_DestroyTexture(*window_output);

    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "blur_gaussian3: texture creation failed: %s\n", SDL_GetError());
}
static inline Uint8 pixel_is_black(Uint32 px, SDL_PixelFormat *fmt)
{
    Uint8 r,g,b;
    SDL_GetRGB(px, fmt, &r, &g, &b);
    // binary image => r==g==b, but tolerate small noise
    return (r < 128);
}

static void dilate_black3x3(SDL_Surface *s, Uint32 *src, Uint32 *dst)
{
    const int w = s->w, h = s->h;
    const int pitch32 = s->pitch / 4;
    SDL_PixelFormat *fmt = s->format;

    const Uint32 BLACK = SDL_MapRGB(fmt, 0, 0, 0);
    const Uint32 WHITE = SDL_MapRGB(fmt, 255, 255, 255);

    for (int y = 0; y < h; y++) {
        int y0 = (y > 0) ? (y - 1) : y;
        int y2 = (y + 1 < h) ? (y + 1) : y;

        for (int x = 0; x < w; x++) {
            int x0 = (x > 0) ? (x - 1) : x;
            int x2 = (x + 1 < w) ? (x + 1) : x;

            // Dilation of black: output black if ANY neighbor is black
            int any_black = 0;
            for (int yy = y0; yy <= y2 && !any_black; yy++) {
                Uint32 *row = src + yy * pitch32;
                for (int xx = x0; xx <= x2; xx++) {
                    if (pixel_is_black(row[xx], fmt)) { any_black = 1; break; }
                }
            }
            dst[y * pitch32 + x] = any_black ? BLACK : WHITE;
        }
    }
}

static void erode_black3x3(SDL_Surface *s, Uint32 *src, Uint32 *dst)
{
    const int w = s->w, h = s->h;
    const int pitch32 = s->pitch / 4;
    SDL_PixelFormat *fmt = s->format;

    const Uint32 BLACK = SDL_MapRGB(fmt, 0, 0, 0);
    const Uint32 WHITE = SDL_MapRGB(fmt, 255, 255, 255);

    for (int y = 0; y < h; y++) {
        int y0 = (y > 0) ? (y - 1) : y;
        int y2 = (y + 1 < h) ? (y + 1) : y;

        for (int x = 0; x < w; x++) {
            int x0 = (x > 0) ? (x - 1) : x;
            int x2 = (x + 1 < w) ? (x + 1) : x;

            // Erosion of black: output black only if ALL neighbors are black
            int all_black = 1;
            for (int yy = y0; yy <= y2 && all_black; yy++) {
                Uint32 *row = src + yy * pitch32;
                for (int xx = x0; xx <= x2; xx++) {
                    if (!pixel_is_black(row[xx], fmt)) { all_black = 0; break; }
                }
            }
            dst[y * pitch32 + x] = all_black ? BLACK : WHITE;
        }
    }
}
// Closing = dilate then erode (fills small white holes/gaps inside letters)
void close_black3x3(SDL_Renderer* renderer,
                    SDL_Surface* master_surface,
                    SDL_Texture **window_output,
                    int iterations)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "close_black3x3: invalid argument\n");
        return;
    }
    if (iterations < 1) iterations = 1;

    if (master_surface->format->BitsPerPixel != 32) {
        fprintf(stderr, "close_black3x3: surface must be 32bpp (got %d)\n",
                master_surface->format->BitsPerPixel);
        return;
    }

    const int pitch32 = master_surface->pitch / 4;
    const size_t count = (size_t)pitch32 * (size_t)master_surface->h;

    Uint32 *bufA = (Uint32*)malloc(count * sizeof(Uint32));
    Uint32 *bufB = (Uint32*)malloc(count * sizeof(Uint32));
    if (!bufA || !bufB) {
        free(bufA); free(bufB);
        fprintf(stderr, "close_black3x3: malloc failed\n");
        return;
    }

    if (SDL_MUSTLOCK(master_surface)) SDL_LockSurface(master_surface);
    Uint32 *dstPixels = (Uint32*)master_surface->pixels;
    memcpy(bufA, dstPixels, count * sizeof(Uint32));

    for (int it = 0; it < iterations; it++) {
        dilate_black3x3(master_surface, bufA, bufB);
        erode_black3x3(master_surface, bufB, bufA);
    }

    memcpy(dstPixels, bufA, count * sizeof(Uint32));
    if (SDL_MUSTLOCK(master_surface)) SDL_UnlockSurface(master_surface);

    free(bufA);
    free(bufB);

    if (*window_output) SDL_DestroyTexture(*window_output);
    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "close_black3x3: texture creation failed: %s\n", SDL_GetError());
}
static inline void sort9_u8(Uint8 v[9])
{
    // simple sorting network-ish bubble (9 is tiny; this is fine)
    for (int i = 0; i < 9; i++) {
        for (int j = i + 1; j < 9; j++) {
            if (v[j] < v[i]) {
                Uint8 t = v[i]; v[i] = v[j]; v[j] = t;
            }
        }
    }
}

void median3x3(SDL_Renderer* renderer,
               SDL_Surface* master_surface,
               SDL_Texture **window_output)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "median3x3: invalid argument\n");
        return;
    }

    if (master_surface->format->BitsPerPixel != 32) {
        fprintf(stderr, "median3x3: surface must be 32bpp (got %d)\n",
                master_surface->format->BitsPerPixel);
        return;
    }

    const int w = master_surface->w;
    const int h = master_surface->h;
    const int pitch32 = master_surface->pitch / 4;
    const size_t count = (size_t)pitch32 * (size_t)h;

    Uint32 *src = (Uint32*)malloc(count * sizeof(Uint32));
    if (!src) {
        fprintf(stderr, "median3x3: malloc failed\n");
        return;
    }

    if (SDL_MUSTLOCK(master_surface)) SDL_LockSurface(master_surface);

    Uint32 *dst = (Uint32*)master_surface->pixels;
    memcpy(src, dst, count * sizeof(Uint32));

    SDL_PixelFormat *fmt = master_surface->format;

    for (int y = 0; y < h; y++) {
        int y0 = (y > 0) ? (y - 1) : y;
        int y1 = y;
        int y2 = (y + 1 < h) ? (y + 1) : y;

        for (int x = 0; x < w; x++) {
            int x0 = (x > 0) ? (x - 1) : x;
            int x1 = x;
            int x2 = (x + 1 < w) ? (x + 1) : x;

            Uint8 gr[9];

            // collect 3x3 neighborhood (convert to grayscale for median)
            int idx = 0;
            const int xs[3] = { x0, x1, x2 };
            const int ys[3] = { y0, y1, y2 };

            for (int yy = 0; yy < 3; yy++) {
                Uint32 *row = src + ys[yy] * pitch32;
                for (int xx = 0; xx < 3; xx++) {
                    Uint8 r,g,b;
                    SDL_GetRGB(row[xs[xx]], fmt, &r, &g, &b);
                    gr[idx++] = (Uint8)(0.299*r + 0.587*g + 0.114*b);
                }
            }

            sort9_u8(gr);
            Uint8 m = gr[4]; // median of 9

            // write back as grayscale pixel (preserve alpha if you want)
            dst[y*pitch32 + x] = SDL_MapRGB(fmt, m, m, m);
        }
    }

    if (SDL_MUSTLOCK(master_surface)) SDL_UnlockSurface(master_surface);
    free(src);

    if (*window_output) SDL_DestroyTexture(*window_output);
    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "median3x3: texture creation failed: %s\n", SDL_GetError());
}

static inline int is_black_px(SDL_PixelFormat *fmt, Uint32 px, Uint8 thr)
{
    Uint8 r,g,b;
    SDL_GetRGB(px, fmt, &r, &g, &b);
    return (r < thr); // after binarize, r==g==b; this is enough
}

static void dilate_black_cross_thresh(SDL_Surface *s, Uint32 *src, Uint32 *dst, Uint8 thr)
{
    const int w = s->w, h = s->h;
    const int pitch32 = s->pitch / 4;
    SDL_PixelFormat *fmt = s->format;

    Uint32 BLACK = SDL_MapRGB(fmt, 0,0,0);
    Uint32 WHITE = SDL_MapRGB(fmt, 255,255,255);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int any = 0;

            // center
            any |= is_black_px(fmt, src[y*pitch32 + x], thr);

            // up/down
            if (!any && y > 0)     any |= is_black_px(fmt, src[(y-1)*pitch32 + x], thr);
            if (!any && y+1 < h)   any |= is_black_px(fmt, src[(y+1)*pitch32 + x], thr);

            // left/right
            if (!any && x > 0)     any |= is_black_px(fmt, src[y*pitch32 + (x-1)], thr);
            if (!any && x+1 < w)   any |= is_black_px(fmt, src[y*pitch32 + (x+1)], thr);

            dst[y*pitch32 + x] = any ? BLACK : WHITE;
        }
    }
}

static void erode_black_cross_thresh(SDL_Surface *s, Uint32 *src, Uint32 *dst, Uint8 thr)
{
    const int w = s->w, h = s->h;
    const int pitch32 = s->pitch / 4;
    SDL_PixelFormat *fmt = s->format;

    Uint32 BLACK = SDL_MapRGB(fmt, 0,0,0);
    Uint32 WHITE = SDL_MapRGB(fmt, 255,255,255);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Erode with cross: all cross neighbors must be black
            int all = 1;

            all &= is_black_px(fmt, src[y*pitch32 + x], thr);
            if (y > 0)     all &= is_black_px(fmt, src[(y-1)*pitch32 + x], thr);
            if (y+1 < h)   all &= is_black_px(fmt, src[(y+1)*pitch32 + x], thr);
            if (x > 0)     all &= is_black_px(fmt, src[y*pitch32 + (x-1)], thr);
            if (x+1 < w)   all &= is_black_px(fmt, src[y*pitch32 + (x+1)], thr);

            dst[y*pitch32 + x] = all ? BLACK : WHITE;
        }
    }
}



void repair_strokes(SDL_Renderer *renderer,
                    SDL_Surface *master_surface,
                    SDL_Texture **window_output)
{
    if (!renderer || !master_surface || !window_output) return;
    if (master_surface->format->BitsPerPixel != 32) return;

    const Uint8 THR = 128;     // black/white cutoff for morphology
    const int iters = 1;       // try 1 first; 2 if still broken

    const int pitch32 = master_surface->pitch / 4;
    const size_t count = (size_t)pitch32 * (size_t)master_surface->h;

    Uint32 *A = (Uint32*)malloc(count * sizeof(Uint32));
    Uint32 *B = (Uint32*)malloc(count * sizeof(Uint32));
    if (!A || !B) { free(A); free(B); return; }

    if (SDL_MUSTLOCK(master_surface)) SDL_LockSurface(master_surface);
    memcpy(A, master_surface->pixels, count * sizeof(Uint32));

    for (int i = 0; i < iters; i++) {
        // Closing with CROSS kernel: bridges gaps but less erasing than 3x3 erosion
        dilate_black_cross_thresh(master_surface, A, B, THR);
        erode_black_cross_thresh(master_surface, B, A, THR);
    }

    memcpy(master_surface->pixels, A, count * sizeof(Uint32));
    if (SDL_MUSTLOCK(master_surface)) SDL_UnlockSurface(master_surface);

    free(A); free(B);

    if (*window_output) SDL_DestroyTexture(*window_output);
    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
}

static inline Uint8 gray_of_px(SDL_PixelFormat *fmt, Uint32 px)
{
    Uint8 r,g,b;
    SDL_GetRGB(px, fmt, &r, &g, &b);
    return (Uint8)(0.299*r + 0.587*g + 0.114*b);
}

static void box_blur_gray_integral(const Uint8 *g, int w, int h, int r, Uint8 *out)
{
    const int W1 = w + 1;
    size_t N = (size_t)(w + 1) * (size_t)(h + 1);

    unsigned long long *I = (unsigned long long*)calloc(N, sizeof(unsigned long long));
    if (!I) return;

    // integral image
    for (int y = 1; y <= h; y++) {
        unsigned long long rowsum = 0;
        const Uint8 *row = g + (y-1)*w;
        for (int x = 1; x <= w; x++) {
            rowsum += row[x-1];
            I[(size_t)y*W1 + x] = I[(size_t)(y-1)*W1 + x] + rowsum;
        }
    }

    for (int y = 0; y < h; y++) {
        int y0 = y - r; if (y0 < 0) y0 = 0;
        int y1 = y + r; if (y1 >= h) y1 = h - 1;
        int iy0 = y0;
        int iy1 = y1 + 1;

        for (int x = 0; x < w; x++) {
            int x0 = x - r; if (x0 < 0) x0 = 0;
            int x1 = x + r; if (x1 >= w) x1 = w - 1;
            int ix0 = x0;
            int ix1 = x1 + 1;

            size_t A = (size_t)iy0*W1 + ix0;
            size_t B = (size_t)iy0*W1 + ix1;
            size_t C = (size_t)iy1*W1 + ix0;
            size_t D = (size_t)iy1*W1 + ix1;

            unsigned long long sum = I[D] - I[B] - I[C] + I[A];
            int area = (x1 - x0 + 1) * (y1 - y0 + 1);
            out[y*w + x] = (Uint8)(sum / (unsigned long long)area);
        }
    }

    free(I);
}

static void sauvola_on_gray(const Uint8 *g, int w, int h,
                            int radius, double k, double R,
                            int bias,
                            Uint8 *out_bw)
{
    const int W1 = w + 1;
    size_t N = (size_t)(w + 1) * (size_t)(h + 1);

    unsigned long long *I  = (unsigned long long*)calloc(N, sizeof(unsigned long long));
    unsigned long long *I2 = (unsigned long long*)calloc(N, sizeof(unsigned long long));
    if (!I || !I2) { free(I); free(I2); return; }

    // integrals
    for (int y = 1; y <= h; y++) {
        unsigned long long rows = 0, rows2 = 0;
        const Uint8 *row = g + (y-1)*w;
        for (int x = 1; x <= w; x++) {
            unsigned long long v = row[x-1];
            rows  += v;
            rows2 += v*v;
            size_t idx = (size_t)y*W1 + x;
            I[idx]  = I[idx - W1]  + rows;
            I2[idx] = I2[idx - W1] + rows2;
        }
    }

    for (int y = 0; y < h; y++) {
        int y0 = y - radius; if (y0 < 0) y0 = 0;
        int y1 = y + radius; if (y1 >= h) y1 = h - 1;
        int iy0 = y0, iy1 = y1 + 1;

        for (int x = 0; x < w; x++) {
            int x0 = x - radius; if (x0 < 0) x0 = 0;
            int x1 = x + radius; if (x1 >= w) x1 = w - 1;
            int ix0 = x0, ix1 = x1 + 1;

            size_t A = (size_t)iy0*W1 + ix0;
            size_t B = (size_t)iy0*W1 + ix1;
            size_t C = (size_t)iy1*W1 + ix0;
            size_t D = (size_t)iy1*W1 + ix1;

            unsigned long long sum  = I[D]  - I[B]  - I[C]  + I[A];
            unsigned long long sum2 = I2[D] - I2[B] - I2[C] + I2[A];

            double area = (double)((x1 - x0 + 1) * (y1 - y0 + 1));
            double mean = (double)sum / area;
            double var  = (double)sum2 / area - mean*mean;
            if (var < 0.0) var = 0.0;
            double std  = sqrt(var);

            double T = mean * (1.0 + k * (std / R - 1.0)) + (double)bias;
            if (T < 0.0) T = 0.0;
            if (T > 255.0) T = 255.0;

            out_bw[y*w + x] = (g[y*w + x] < (int)T) ? 0 : 255;
        }
    }

    free(I);
    free(I2);
}

static double black_ratio_bw(const Uint8 *bw, int w, int h)
{
    long long black = 0;
    long long total = (long long)w * (long long)h;
    for (long long i = 0; i < total; i++) if (bw[i] == 0) black++;
    return total ? (double)black / (double)total : 0.0;
}

/*
 * Unified binarize: background normalization + Sauvola + fail-safe.
 * Signature matches your command handler: no manual threshold.
 */
static void mean_std_u8(const Uint8 *a, int n, double *mean, double *std)
{
    double s = 0.0, s2 = 0.0;
    for (int i = 0; i < n; i++) {
        double v = (double)a[i];
        s  += v;
        s2 += v*v;
    }
    double m = s / (double)n;
    double var = s2 / (double)n - m*m;
    if (var < 0.0) var = 0.0;
    *mean = m;
    *std  = sqrt(var);
}

void binarize(SDL_Renderer* renderer,
              SDL_Surface* master_surface,
              SDL_Texture **window_output)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "binarize: invalid argument\n");
        return;
    }

    if (master_surface->format->BitsPerPixel != 32) {
        fprintf(stderr, "binarize: surface must be 32bpp\n");
        return;
    }

    const int w = master_surface->w;
    const int h = master_surface->h;
    const int pitch32 = master_surface->pitch / 4;
    SDL_PixelFormat *fmt = master_surface->format;

    Uint8 *g  = (Uint8*)malloc((size_t)w * (size_t)h);
    Uint8 *bg = (Uint8*)malloc((size_t)w * (size_t)h);
    Uint8 *gn = (Uint8*)malloc((size_t)w * (size_t)h);
    Uint8 *bw = (Uint8*)malloc((size_t)w * (size_t)h);

    if (!g || !bg || !gn || !bw) {
        free(g); free(bg); free(gn); free(bw);
        return;
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint32 *pix = (Uint32*)master_surface->pixels;

    // 1) grayscale buffer
    for (int y = 0; y < h; y++) {
        Uint32 *row = pix + y * pitch32;
        for (int x = 0; x < w; x++) {
            g[y*w + x] = gray_of_px(fmt, row[x]);
        }
    }

    // --- parameters (auto, but stable) ---
    int minDim = (w < h) ? w : h;

    // background radius
    int bg_r = (int)(0.020 * (double)minDim);
    if (bg_r < 14) bg_r = 14;
    if (bg_r > 32) bg_r = 32;

    // sauvola radius
    int r = (int)(0.011 * (double)minDim);
    if (r < 7) r = 7;
    if (r > 18) r = 18;

    const double R = 128.0;
    // ------------------------------------

    // 2) estimate background
    box_blur_gray_integral(g, w, h, bg_r, bg);

    // 3) normalize: gray - bg + 128
    for (int i = 0; i < w*h; i++) {
        int v = (int)g[i] - (int)bg[i] + 128;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        gn[i] = (Uint8)v;
    }

    // 4) decide sensitivity
    double mgn, sgn;
    mean_std_u8(gn, w*h, &mgn, &sgn);

    double k;
    int bias;

    if (sgn < 10.0) {
        k = 0.20;
        bias = 24;
    }
    else if (sgn < 18.0) {
        k = 0.28;
        bias = 14;
    }
    else {
        k = 0.40;
        bias = 6;
    }

    // 5) Sauvola
    sauvola_on_gray(gn, w, h, r, k, R, bias, bw);

    // 6) fail-safes
    double br = black_ratio_bw(bw, w, h);

    if (br < 0.002) {
        sauvola_on_gray(gn, w, h, r, 0.15, R, bias + 12, bw);
        br = black_ratio_bw(bw, w, h);
    }

    if (br < 0.002) {
        sauvola_on_gray(gn, w, h, 7, 0.10, R, bias + 20, bw);
    }

    br = black_ratio_bw(bw, w, h);
    if (br > 0.40) {
        sauvola_on_gray(gn, w, h, r, 0.55, R, bias - 6, bw);
    }

    // 7) write to surface
    Uint32 BLACK = SDL_MapRGB(fmt, 0, 0, 0);
    Uint32 WHITE = SDL_MapRGB(fmt, 255, 255, 255);

    for (int y = 0; y < h; y++) {
        Uint32 *row = pix + y * pitch32;
        for (int x = 0; x < w; x++) {
            row[x] = (bw[y*w + x] == 0) ? BLACK : WHITE;
        }
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_UnlockSurface(master_surface);

    free(g);
    free(bg);
    free(gn);
    free(bw);

    if (*window_output)
        SDL_DestroyTexture(*window_output);

    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "binarize: texture creation failed: %s\n", SDL_GetError());
}