#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "rot.h"  

typedef struct tColorRGBA {
    Uint8 r, g, b, a;
} tColorRGBA;

#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define GUARD_ROWS  (2)
#define VALUE_LIMIT (0.001)

static void _rotozoomSurfaceSizeTrig(int width, int height, double angle,
                                     double zoomx, double zoomy,
                                     int *dstwidth, int *dstheight,
                                     double *canglezoom, double *sanglezoom)
{
    double x, y, cx, cy, sx, sy;
    double radangle;
    int dstwidthhalf, dstheighthalf;

    radangle = angle * (M_PI / 180.0);
    *sanglezoom = sin(radangle);
    *canglezoom = cos(radangle);
    *sanglezoom *= zoomx;
    *canglezoom *= zoomy;

    x = (double)(width / 2);
    y = (double)(height / 2);

    cx = *canglezoom * x;
    cy = *canglezoom * y;
    sx = *sanglezoom * x;
    sy = *sanglezoom * y;

    dstwidthhalf = MAX((int)ceil(MAX(MAX(MAX(fabs(cx + sy), fabs(cx - sy)),
                                     fabs(-cx + sy)), fabs(-cx - sy))), 1);
    dstheighthalf = MAX((int)ceil(MAX(MAX(MAX(fabs(sx + cy), fabs(sx - cy)),
                                      fabs(-sx + cy)), fabs(-sx - cy))), 1);

    *dstwidth  = 2 * dstwidthhalf;
    *dstheight = 2 * dstheighthalf;
}

static void _transformSurfaceRGBA(SDL_Surface *src, SDL_Surface *dst,
                                  int cx, int cy, int isin, int icos,
                                  int flipx, int flipy, int smooth)
{
    int x, y, t1, t2, dx, dy, xd, yd, sdx, sdy, ax, ay, ex, ey, sw, sh;
    tColorRGBA c00, c01, c10, c11, cswap;
    tColorRGBA *pc, *sp;
    int gap;

    xd = ((src->w - dst->w) << 15);
    yd = ((src->h - dst->h) << 15);
    ax = (cx << 16) - (icos * cx);
    ay = (cy << 16) - (isin * cx);

    sw = src->w - 1;
    sh = src->h - 1;

    pc = (tColorRGBA *)dst->pixels;
    gap = dst->pitch - dst->w * 4;

    if (smooth) {
        for (y = 0; y < dst->h; y++) {
            dy  = cy - y;
            sdx = (ax + (isin * dy)) + xd;
            sdy = (ay - (icos * dy)) + yd;

            for (x = 0; x < dst->w; x++) {
                dx = (sdx >> 16);
                dy = (sdy >> 16);

                if (flipx) dx = sw - dx;
                if (flipy) dy = sh - dy;

                if ((dx > -1) && (dy > -1) && (dx < (src->w - 1)) && (dy < (src->h - 1))) {
                    sp  = (tColorRGBA *)src->pixels;
                    sp += (src->pitch / 4) * dy;
                    sp += dx;

                    c00 = *sp; sp += 1;
                    c01 = *sp; sp += (src->pitch / 4);
                    c11 = *sp; sp -= 1;
                    c10 = *sp;

                    if (flipx) {
                        cswap = c00; c00 = c01; c01 = cswap;
                        cswap = c10; c10 = c11; c11 = cswap;
                    }
                    if (flipy) {
                        cswap = c00; c00 = c10; c10 = cswap;
                        cswap = c01; c01 = c11; c11 = cswap;
                    }

                    ex = (sdx & 0xffff);
                    ey = (sdy & 0xffff);

                    t1 = ((((c01.r - c00.r) * ex) >> 16) + c00.r) & 0xff;
                    t2 = ((((c11.r - c10.r) * ex) >> 16) + c10.r) & 0xff;
                    pc->r = (((t2 - t1) * ey) >> 16) + t1;

                    t1 = ((((c01.g - c00.g) * ex) >> 16) + c00.g) & 0xff;
                    t2 = ((((c11.g - c10.g) * ex) >> 16) + c10.g) & 0xff;
                    pc->g = (((t2 - t1) * ey) >> 16) + t1;

                    t1 = ((((c01.b - c00.b) * ex) >> 16) + c00.b) & 0xff;
                    t2 = ((((c11.b - c10.b) * ex) >> 16) + c10.b) & 0xff;
                    pc->b = (((t2 - t1) * ey) >> 16) + t1;

                    t1 = ((((c01.a - c00.a) * ex) >> 16) + c00.a) & 0xff;
                    t2 = ((((c11.a - c10.a) * ex) >> 16) + c10.a) & 0xff;
                    pc->a = (((t2 - t1) * ey) >> 16) + t1;
                }

                sdx += icos;
                sdy += isin;
                pc++;
            }

            pc = (tColorRGBA *)((Uint8 *)pc + gap);
        }
    } else {
        for (y = 0; y < dst->h; y++) {
            dy  = cy - y;
            sdx = (ax + (isin * dy)) + xd;
            sdy = (ay - (icos * dy)) + yd;

            for (x = 0; x < dst->w; x++) {
                dx = (short)(sdx >> 16);
                dy = (short)(sdy >> 16);

                if (flipx) dx = (src->w - 1) - dx;
                if (flipy) dy = (src->h - 1) - dy;

                if ((dx >= 0) && (dy >= 0) && (dx < src->w) && (dy < src->h)) {
                    sp = (tColorRGBA *)((Uint8 *)src->pixels + src->pitch * dy);
                    sp += dx;
                    *pc = *sp;
                }

                sdx += icos;
                sdy += isin;
                pc++;
            }

            pc = (tColorRGBA *)((Uint8 *)pc + gap);
        }
    }
}

SDL_Surface *GFX_rotozoomSurface(SDL_Surface *src, double angle, double zoom, int smooth)
{
    return GFX_rotozoomSurfaceXY(src, angle, zoom, zoom, smooth);
}

SDL_Surface *GFX_rotozoomSurfaceXY(SDL_Surface *src, double angle,
                                   double zoomx, double zoomy, int smooth)
{
    SDL_Surface *rz_src = NULL;
    SDL_Surface *rz_dst = NULL;
    int src_converted = 0;

    double zoominv;
    double sanglezoom, canglezoom, sanglezoominv, canglezoominv;
    int dstwidth, dstheight;
    int dstwidthhalf, dstheighthalf;
    int flipx, flipy;

    if (!src) return NULL;

    /* We only keep the ROTATION path (angle must be non-zero) */
    if (fabs(angle) <= VALUE_LIMIT) {
        SDL_SetError("rotozoom_only: angle=0 path removed");
        return NULL;
    }

    /* Convert source to 32-bit if needed (same idea as SDL2_gfx) */
    if (src->format->BitsPerPixel == 32) {
        rz_src = src;
        src_converted = 0;
    } else {
        rz_src = SDL_CreateRGBSurface(SDL_SWSURFACE, src->w, src->h, 32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                                      0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#else
                                      0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#endif
        );
        if (!rz_src) return NULL;

        SDL_BlitSurface(src, NULL, rz_src, NULL);
        src_converted = 1;
    }

    /* Handle flips + clamp zoom like original */
    flipx = (zoomx < 0.0); if (flipx) zoomx = -zoomx;
    flipy = (zoomy < 0.0); if (flipy) zoomy = -zoomy;

    if (zoomx < VALUE_LIMIT) zoomx = VALUE_LIMIT;
    if (zoomy < VALUE_LIMIT) zoomy = VALUE_LIMIT;

    zoominv = 65536.0 / (zoomx * zoomx);

    /* Determine target size + trig */
    _rotozoomSurfaceSizeTrig(rz_src->w, rz_src->h, angle, zoomx, zoomy,
                             &dstwidth, &dstheight, &canglezoom, &sanglezoom);

    /* Compute inverse factors (fixed-point expected downstream) */
    sanglezoominv = sanglezoom * zoominv;
    canglezoominv = canglezoom * zoominv;

    dstwidthhalf  = dstwidth / 2;
    dstheighthalf = dstheight / 2;

    /* Create dst (32-bit, preserve masks from rz_src like original) */
    rz_dst = SDL_CreateRGBSurface(SDL_SWSURFACE,
                                  dstwidth, dstheight + GUARD_ROWS,
                                  32,
                                  rz_src->format->Rmask,
                                  rz_src->format->Gmask,
                                  rz_src->format->Bmask,
                                  rz_src->format->Amask);
    if (!rz_dst) {
        if (src_converted) SDL_FreeSurface(rz_src);
        return NULL;
    }

    /* Hide guard rows */
    rz_dst->h = dstheight;

    if (SDL_MUSTLOCK(rz_src)) SDL_LockSurface(rz_src);

    _transformSurfaceRGBA(rz_src, rz_dst,
                          dstwidthhalf, dstheighthalf,
                          (int)(sanglezoominv), (int)(canglezoominv),
                          flipx, flipy, smooth);

    if (SDL_MUSTLOCK(rz_src)) SDL_UnlockSurface(rz_src);

    if (src_converted) SDL_FreeSurface(rz_src);

    return rz_dst;
}