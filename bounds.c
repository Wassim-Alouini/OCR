#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include "bounds.h"

static inline int is_adjacent(Coord a, Coord b) {
    return (abs(a.x - b.x) <= 1 && abs(a.y - b.y) <= 1);
}

Coord **find_blobs(SDL_Surface *surface, int *blob_count, int **blob_sizes_out) {
    if (!surface || !blob_count || !blob_sizes_out) return NULL;

    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    Uint32 *pixels = (Uint32 *)surface->pixels;
    SDL_PixelFormat *fmt = surface->format;
    int w = surface->w, h = surface->h;
    Uint8 r, g, b;

    Coord **res = NULL;      // list of blobs
    int *blob_sizes = NULL;  // number of pixels in each blob
    *blob_count = 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Uint32 pixel = pixels[y * w + x];
            SDL_GetRGB(pixel, fmt, &r, &g, &b);

            if (r < 10 && g < 10 && b < 10) { // black pixel found
                Coord p = {x, y};
                int added = 0;

                for (int i = 0; i < *blob_count && !added; i++) {
                    for (int j = 0; j < blob_sizes[i]; j++) {
                        if (is_adjacent(p, res[i][j])) {
                            blob_sizes[i]++;
                            res[i] = realloc(res[i], blob_sizes[i] * sizeof(Coord));
                            res[i][blob_sizes[i] - 1] = p;
                            added = 1;
                            break;
                        }
                    }
                }

                if (!added) {
                    (*blob_count)++;
                    res = realloc(res, (*blob_count) * sizeof(Coord *));
                    blob_sizes = realloc(blob_sizes, (*blob_count) * sizeof(int));

                    blob_sizes[*blob_count - 1] = 1;
                    res[*blob_count - 1] = malloc(sizeof(Coord));
                    res[*blob_count - 1][0] = p;
                }
            }
        }
    }

    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);

    *blob_sizes_out = blob_sizes;
    return res;
}

Box *compute_blob_boxes(Coord **blobs, int *blob_sizes, int blob_count) {
    if (!blobs || !blob_sizes || blob_count <= 0) return NULL;

    Box *boxes = malloc(blob_count * sizeof(Box));

    for (int i = 0; i < blob_count; i++) {
        int minx = blobs[i][0].x;
        int maxx = blobs[i][0].x;
        int miny = blobs[i][0].y;
        int maxy = blobs[i][0].y;

        for (int j = 1; j < blob_sizes[i]; j++) {
            int x = blobs[i][j].x;
            int y = blobs[i][j].y;
            if (x < minx) minx = x;
            if (x > maxx) maxx = x;
            if (y < miny) miny = y;
            if (y > maxy) maxy = y;
        }

        boxes[i].x = minx;
        boxes[i].y = miny;
        boxes[i].w = maxx - minx + 1;
        boxes[i].h = maxy - miny + 1;
    }

    return boxes;
}

void draw_boxes(SDL_Surface *surface, Box *boxes, int count, Uint8 r, Uint8 g, Uint8 b) {
    if (!surface || !boxes || count <= 0) return;

    Uint32 color = SDL_MapRGB(surface->format, r, g, b);

    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    Uint32 *pixels = (Uint32 *)surface->pixels;
    int w = surface->w;

    for (int i = 0; i < count; i++) {
        Box box = boxes[i];

        for (int x = box.x; x < box.x + box.w; x++) {
            if (x >= 0 && x < surface->w) {
                if (box.y >= 0 && box.y < surface->h)
                    pixels[box.y * w + x] = color;
                if (box.y + box.h - 1 >= 0 && box.y + box.h - 1 < surface->h)
                    pixels[(box.y + box.h - 1) * w + x] = color;
            }
        }

        for (int y = box.y; y < box.y + box.h; y++) {
            if (y >= 0 && y < surface->h) {
                if (box.x >= 0 && box.x < surface->w)
                    pixels[y * w + box.x] = color;
                if (box.x + box.w - 1 >= 0 && box.x + box.w - 1 < surface->w)
                    pixels[y * w + (box.x + box.w - 1)] = color;
            }
        }
    }

    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
}
