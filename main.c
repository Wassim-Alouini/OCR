#include <stdio.h>
#include "image_loader.h"
#include "window_manager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <string.h>
#include "cmd_window.h"


void binarize(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture** window_output, Uint8 threshold);
void rotate_and_render(SDL_Renderer* renderer, double angle, SDL_Surface* master_surface, SDL_Window* window, SDL_Texture** window_output);
void apply_grayscale(SDL_Renderer* renderer, SDL_Surface* master_surface, SDL_Texture** window_output);
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
	}

	if(strcmp(command, "grayscale") == 0)
	{
	    apply_grayscale(renderer, master_surface, window_output);
	}
	if(strcmp(command, "binarize") == 0)
	{
	    binarize(renderer, master_surface, window_output, cmd);
	}

    }

    return 1;
}

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


    //init image "input.bmp" as surface DONE
    //init renderer DONE
    //resize window (trig calculate maximum bounding box after rotation) DONE
    //create texture (of same new window size) DONE
    //create texture from surface  DONE
    //copy surfacetexture to window texture DONE

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

    SDL_Texture *window_texture = NULL;
    initialize_window_texture(&window_texture, renderer, maxw, maxh);

    SDL_Texture *image_texture = NULL;
    create_texture_from_surface(&image_texture, renderer, master_surface);

    render_texture_rotated(renderer, image_texture, window_texture,maxw, maxh,w, h,  angle);
    *window_output = window_texture;

    terminate_texture(image_texture);
}

void apply_grayscale(SDL_Renderer* renderer,
                     SDL_Surface* master_surface,
                     SDL_Texture** window_output)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "apply_grayscale: invalid argument\n");
        return;
    }

    // Lock the surface if needed
    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint8 r, g, b;
    Uint32 *pixels = (Uint32 *)master_surface->pixels;
    SDL_PixelFormat *fmt = master_surface->format;
    int total_pixels = master_surface->w * master_surface->h;

    for (int i = 0; i < total_pixels; i++) {
        SDL_GetRGB(pixels[i], fmt, &r, &g, &b);

        // Standard luminance grayscale conversion
        Uint8 gray = (Uint8)(0.299 * r + 0.587 * g + 0.114 * b);

        pixels[i] = SDL_MapRGB(fmt, gray, gray, gray);
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_UnlockSurface(master_surface);

    // Destroy old texture
    if (*window_output)
        SDL_DestroyTexture(*window_output);

    // Create new texture from the modified surface
    *window_output = SDL_CreateTextureFromSurface(renderer, master_surface);
    if (!*window_output)
        fprintf(stderr, "apply_grayscale: texture creation failed: %s\n", SDL_GetError());
}

void binarize(SDL_Renderer* renderer,
		SDL_Surface* master_surface,
                SDL_Texture **window_output,
                Uint8 threshold)
{
    if (!renderer || !master_surface || !window_output) {
        fprintf(stderr, "apply_binarization: invalid argument\n");
        return;
    }

    if (SDL_MUSTLOCK(master_surface))
        SDL_LockSurface(master_surface);

    Uint8 r, g, b;
    Uint32 *pixels = (Uint32 *)master_surface->pixels;
    SDL_PixelFormat* fmt = master_surface->format;
    int total_pixels = master_surface->w * master_surface->h;

    for (int i = 0; i < total_pixels; i++) {
        SDL_GetRGB(pixels[i], fmt, &r, &g, &b);
        // since grayscale: r==g==b
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
