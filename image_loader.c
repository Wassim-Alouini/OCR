#include "image_loader.h"

void load_image(SDL_Surface** surface_out, const char* file)
{
    *surface_out = SDL_LoadBMP(file);
}

void create_texture_from_surface(SDL_Texture** texture_out, SDL_Renderer* renderer, SDL_Surface* surface)
{
    *texture_out = SDL_CreateTextureFromSurface(renderer, surface);
    if(*texture_out == NULL)
    {
	errx(EXIT_FAILURE, "Error Texture");
    }
}

void render_texture_rotated
(SDL_Renderer* renderer,
 SDL_Texture* texture, SDL_Texture* master_texture,
 int maxw, int maxh,
 int srcw, int srch,
 const double angle)
{
    SDL_Texture* temp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, maxw, maxh);
    SDL_SetRenderTarget(renderer, temp);
    SDL_Rect dst;
    dst.w = srcw;
    dst.h = srch;
    dst.x = (maxw - srcw) / 2;
    dst.y = (maxh - srch) / 2;
    int err = SDL_RenderCopyEx(renderer, texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
    if(err)
    {
	errx(EXIT_FAILURE, "Error rendering");
    }
    SDL_SetRenderTarget(renderer, master_texture);
    SDL_RenderCopy(renderer, temp, NULL, NULL);
    SDL_DestroyTexture(temp);
    SDL_SetRenderTarget(renderer, NULL);
}

