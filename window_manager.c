#include "window_manager.h"

//Initialize a window
void initialize_window(SDL_Window **window)
{
    int init = SDL_Init(SDL_INIT_VIDEO);
    if(init)
    {
	errx(EXIT_FAILURE, "Initialization Error");
    }
    *window = SDL_CreateWindow
	    ("OCR", 
	     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
	     600, 600, 
	     0);
    if(*window == NULL)
    {
	errx(EXIT_FAILURE, "Error creating window");
    }
}
//Window texture being the texture used as render target for display
void initialize_window_texture(SDL_Texture **texture, SDL_Renderer *renderer, int w, int h)
{
    *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if(*texture == NULL)
    {
	errx(EXIT_FAILURE, "Error creating texture");
    }
}

//Initialize a renderer
void initialize_renderer(SDL_Renderer **renderer, SDL_Window *window)
{
    *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
    if(*renderer == NULL)
    {
	errx(EXIT_FAILURE, "Error creating renderer");
    }
}

//Wrapper for SDL_DestroyWindow
void terminate_window(SDL_Window *window)
{
    if(window == NULL)
    {
	errx(EXIT_FAILURE, "Parameter window is NULL");
    }
    SDL_DestroyWindow(window);
}

//Wrapper for SDL_DestroyRenderer
void terminate_renderer(SDL_Renderer *renderer)
{
    if(renderer == NULL)
    {
	errx(EXIT_FAILURE, "Parameter renderer is NULL");
    }
    SDL_DestroyRenderer(renderer);
}

//Wrapper for SDL_DestroyTexture
void terminate_texture(SDL_Texture *texture)
{
    if(texture == NULL)
    {
	errx(EXIT_FAILURE, "Parameter texture is NULL");
    }
    SDL_DestroyTexture(texture);
}
