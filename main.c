#include <stdio.h>
#include "image_loader.h"
#include "window_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <string.h>
#include "cmd_window.h"
#include "bounds.h"

void binarize
(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture** window_output, Uint8 threshold);
void rotate_and_render
(SDL_Renderer* renderer, double angle, SDL_Surface* master_surface, SDL_Window* window, SDL_Texture** window_output);
void apply_grayscale
(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture** window_output);

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
        printf("Saved %s\n", filename);
	
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
int event_handler(SDL_Renderer* renderer,  SDL_Surface* master_surface, SDL_Window* window, SDL_Texture** window_output)
{
    SDL_Event current;
    while (SDL_PollEvent(&current))
    {
	switch(current.type)
	{
	    case SDL_QUIT:
		return 0;
		break;
	    default:
		break;
	
	}
	char* command = "0";
	int cmd = cmdwindow_handle_event(&current, &command);

	if(strcmp(command, "rotate") == 0)
	{
	    rotate_and_render(renderer, cmd, master_surface, window, window_output);

	    //TODO
	    //Buggy part
	    //applying the rotation on surface seems to cause issues,
	    //perhaps messes with format of pixels


	    //apply_rotation_to_surface(renderer, *window_output, &master_surface);
	    //SDL_Surface* s32 = SDL_ConvertSurfaceFormat(master_surface, SDL_PIXELFORMAT_ARGB8888, 0);
	    //if(s32)
	    //{
		//SDL_FreeSurface(master_surface);
		//master_surface = s32;
	    //}
	}

	if(strcmp(command, "grayscale") == 0)
	{
	    apply_grayscale(renderer, master_surface, window_output);
	}
	if(strcmp(command, "binarize") == 0)
	{
	    binarize(renderer, master_surface, window_output, cmd);
	}
	if(strcmp(command, "boxes") == 0)
	{
	    int blob_count = 0;
	    int* blob_sizes = NULL;
	    Coord** blobs = find_blobs_rec(master_surface, &blob_count, &blob_sizes);

	    printf("Found %d blobs\n", blob_count);

	    Box *boxes = compute_blob_boxes(blobs, blob_sizes, blob_count);
	    int newcount;
	    Box* myboxes = extract(boxes, blob_count, &newcount);

	    draw_boxes(master_surface, myboxes, newcount, 255, 0, 0);

	    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);

	    extract_boxes_to_bmp(master_surface, myboxes, newcount, "images");
	}

    }
    return 1;
}

//Update loop, renders the texture while running.
void update(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Window* window, SDL_Texture** texture)
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
int main() 
{

    TTF_Init();

    const char* input = "input.png";
    const char* output = "output.bmp";

    char command[256];
    snprintf(command, sizeof(command), "magick \"%s\" \"%s\"", input, output);

    int res = system(command);

    if(res)
    {
	errx(EXIT_FAILURE, "Couldn't convert image file");
    }


    SDL_Window *window = NULL;
    initialize_window(&window);

    TTF_Init();

    cmdwindow_init();
    SDL_Surface *master_surface = NULL;
    load_image(&master_surface, "output.bmp");

    SDL_Renderer *renderer = NULL;
    initialize_renderer(&renderer, window);

    double w = (double) (master_surface -> w);
    double h = (double) (master_surface -> h);

    double angle = 0;
    double rad = angle * M_PI / 180.0;
    double bw = fabs(w * cos(rad)) + fabs(h * sin(rad));
    double bh = fabs(w * sin(rad)) + fabs(h * cos(rad));

    int maxw = (int)ceil(bw);
    int maxh = (int)ceil(bh);

    SDL_SetWindowSize(window, maxw, maxh);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_Texture *window_texture = NULL;
    initialize_window_texture(&window_texture, renderer, maxw, maxh);

    SDL_Texture *image_texture = NULL;
    create_texture_from_surface(&image_texture, renderer, master_surface);

    render_texture_rotated(renderer, image_texture, window_texture,maxw, maxh,w, h,  angle);
    SDL_RenderCopy(renderer, window_texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    update(renderer,master_surface, window, &window_texture);

    terminate_window(window);
    terminate_renderer(renderer);
    terminate_texture(image_texture);
    terminate_texture(window_texture);
    SDL_FreeSurface(master_surface);
    SDL_Quit();
    printf("Window successfully created!\n");
    return EXIT_SUCCESS;
}

//Rotates and renders a texture.
void rotate_and_render(SDL_Renderer* renderer, double angle, SDL_Surface* master_surface, SDL_Window* window, SDL_Texture** window_output)
{
    double w = (double) (master_surface -> w);
    double h = (double) (master_surface -> h);

    double rad = angle * M_PI / 180.0;
    double bw = fabs(w * cos(rad)) + fabs(h * sin(rad));
    double bh = fabs(w * sin(rad)) + fabs(h * cos(rad));

    int maxw = (int)ceil(bw);
    int maxh = (int)ceil(bh);

    SDL_SetWindowSize(window, maxw, maxh);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    if (*window_output) 
    {
        SDL_DestroyTexture(*window_output);
        *window_output = NULL;
    }

    SDL_Texture *window_texture = NULL;
    initialize_window_texture(&window_texture, renderer, maxw, maxh);

    SDL_Texture *image_texture = NULL;
    create_texture_from_surface(&image_texture, renderer, master_surface);

    render_texture_rotated(renderer, image_texture, window_texture,maxw, maxh,w, h,  angle);
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
void binarize(SDL_Renderer* renderer,
		SDL_Surface* master_surface,
                SDL_Texture **window_output,
                Uint8 threshold)
{
    if (!renderer || !master_surface || !window_output) 
    { 
        fprintf(stderr, "apply_binarization: invalid argument\n");
        return;
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint8 r, g, b;
    Uint32 *pixels = (Uint32 *)master_surface->pixels;
    SDL_PixelFormat* fmt = master_surface->format;
    int total_pixels = master_surface->w * master_surface->h;

    for (int i = 0; i < total_pixels; i++) 
    {
        SDL_GetRGB(pixels[i], fmt, &r, &g, &b);
        Uint8 bw = (r < threshold) ? 0 : 255;
        pixels[i] = SDL_MapRGB(fmt, bw, bw, bw);
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_UnlockSurface(master_surface);

    if (*window_output)
        SDL_DestroyTexture(*window_output);

    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "binarize: texture creation failed: %s\n", SDL_GetError());
}
