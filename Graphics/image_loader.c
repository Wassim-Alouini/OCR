#include "image_loader.h"

//Load BMP file as surface
void load_image(SDL_Surface** surface_out, const char* file)
{
    if (!surface_out || !file)
        errx(EXIT_FAILURE, "load_image: invalid argument");

    SDL_Surface* raw = SDL_LoadBMP(file);
    if (!raw)
        errx(EXIT_FAILURE, "load_image: SDL_LoadBMP failed: %s", SDL_GetError());

    // Avoid RLE surprises when doing direct pixel access later
    SDL_SetSurfaceRLE(raw, 0);

    // Force the format your pixel code assumes (Uint32 per pixel)
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);

    if (!conv)
        errx(EXIT_FAILURE, "load_image: SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());

    *surface_out = conv;
}

//Wrapper for CreateTextureFromSurface
void create_texture_from_surface(SDL_Texture** texture_out, SDL_Renderer* renderer, SDL_Surface* surface)
{
    *texture_out = SDL_CreateTextureFromSurface(renderer, surface);
    if(*texture_out == NULL)
    {
	errx(EXIT_FAILURE, "Error Texture");
    }
}

//Rotate master texture and render it on window texture
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

