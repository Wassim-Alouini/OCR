#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct {
    int x;
    int y;
} Coord;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Box;

Coord **find_blobs(SDL_Surface *surface, int *blob_count, int **blob_sizes_out);
Box *compute_blob_boxes(Coord **blobs, int *blob_sizes, int blob_count);
void draw_boxes(SDL_Surface *surface, Box *boxes, int count, Uint8 r, Uint8 g, Uint8 b);
