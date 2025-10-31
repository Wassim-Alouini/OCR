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
int same_row(Box a, Box b, int max_y_diff);
Box* extract(Box* boxes, int n, int* out_n);
double compute_average_horizontal_gap(Box *boxes, int n, int max_y_diff);
Box* differentiate_grid_list_and_words(Box* sorted, int n, int* out_count);
void sort_boxes(Box* boxes, size_t size);
int boxcmp(const void* a, const void* b);
Coord **find_blobs_rec(SDL_Surface *surface, int *blob_count, int **blob_sizes_out);
void flood_fill(SDL_Surface *surface, int x, int y, int **visited, Coord **blob, int *blob_size);
Coord **find_blobs(SDL_Surface *surface, int *blob_count, int **blob_sizes_out);
Box *compute_blob_boxes(Coord **blobs, int *blob_sizes, int blob_count);
void draw_boxes(SDL_Surface *surface, Box *boxes, int count, Uint8 r, Uint8 g, Uint8 b);
