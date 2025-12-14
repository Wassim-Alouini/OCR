#pragma once
#include <SDL2/SDL.h>

#ifndef SMOOTHING_ON
#define SMOOTHING_ON 1
#endif
#ifndef SMOOTHING_OFF
#define SMOOTHING_OFF 0
#endif

SDL_Surface* GFX_rotozoomSurface(SDL_Surface* src, double angle, double zoom,
                                 int smooth);
SDL_Surface* GFX_rotozoomSurfaceXY(SDL_Surface* src, double angle, double zoomx,
                                   double zoomy, int smooth);
