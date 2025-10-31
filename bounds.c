#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include "bounds.h"
#include <limits.h>

//Are points a and b adjacent ? utility
static inline int is_adjacent(Coord a, Coord b) 
{
    return (abs(a.x - b.x) <= 1 && abs(a.y - b.y) <= 1);
}

//Recursive algorithm to find blobs of black pixels in surface,
//array of array of points blob and its size blob_size
void flood_fill(SDL_Surface *surface, int x, int y,
                int **visited, Coord **blob, int *blob_size) 
{
    int w = surface->w;
    int h = surface->h;
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    if (visited[y][x]) return;
    int stride = surface->pitch / 4;
    Uint32 *pixels = (Uint32 *)surface->pixels;
    SDL_PixelFormat *fmt = surface->format;
    Uint8 r, g, b;
    Uint32 pixel = pixels[y * stride + x];
    SDL_GetRGB(pixel, fmt, &r, &g, &b);

    if (!(r < 10 && g < 10 && b < 10)) return;
    visited[y][x] = 1;

    (*blob_size)++;
    *blob = realloc(*blob, (*blob_size) * sizeof(Coord));
    (*blob)[*blob_size - 1].x = x;
    (*blob)[*blob_size - 1].y = y;

    for (int dy = -1; dy <= 1; dy++) 
    {
        for (int dx = -1; dx <= 1; dx++) 
	{
            if (dx == 0 && dy == 0) continue;
            flood_fill(surface, x + dx, y + dy, visited, blob, blob_size);
        }
    }
}

//main function of the blob finding algorithm, reads surface pixels, when black pixel
//is encountered run flood_fill to visit all the blob, visited pixels are later ignored.
Coord **find_blobs_rec(SDL_Surface *surface, int *blob_count, int **blob_sizes_out) 
{
    if (!surface || !blob_count || !blob_sizes_out) return NULL;

    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    int w = surface->w, h = surface->h;
    int **visited = malloc(h * sizeof(int *));
    for (int y = 0; y < h; y++) 
    {
        visited[y] = calloc(w, sizeof(int));
    }

    Coord **res = NULL;
    int *blob_sizes = NULL;
    *blob_count = 0;

    for (int y = 0; y < h; y++) 
    {
        for (int x = 0; x < w; x++) 
	{
            if (visited[y][x]) continue;

            Uint32 *pixels = (Uint32 *)surface->pixels;
            SDL_PixelFormat *fmt = surface->format;
            Uint8 r, g, b;
	    int stride = surface->pitch / 4;
            Uint32 pixel = pixels[y * stride + x];
            SDL_GetRGB(pixel, fmt, &r, &g, &b);

            if (r < 10 && g < 10 && b < 10) 
	    {
                (*blob_count)++;
                res = realloc(res, (*blob_count) * sizeof(Coord *));
                blob_sizes = realloc(blob_sizes, (*blob_count) * sizeof(int));
                res[*blob_count - 1] = NULL;
                blob_sizes[*blob_count - 1] = 0;

                flood_fill(surface, x, y, visited,
                           &res[*blob_count - 1],
                           &blob_sizes[*blob_count - 1]);
            }
        }
    }

    for (int y = 0; y < h; y++) free(visited[y]);
    free(visited);

    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);

    *blob_sizes_out = blob_sizes;
    return res;
}

//first attempt at a blob finding algorithm, naive iterative approach.
Coord **find_blobs(SDL_Surface *surface, int *blob_count, int **blob_sizes_out) 
{
    if (!surface || !blob_count || !blob_sizes_out) return NULL;

    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    Uint32 *pixels = (Uint32 *)surface->pixels;
    SDL_PixelFormat *fmt = surface->format;
    int w = surface->w, h = surface->h;
    Uint8 r, g, b;

    Coord **res = NULL;
    int *blob_sizes = NULL;
    *blob_count = 0;
    int stride = surface->pitch / 4;

    for (int y = 0; y < h; y++) 
    {
        for (int x = 0; x < w; x++) 
	{
            Uint32 pixel = pixels[y * stride + x];
            SDL_GetRGB(pixel, fmt, &r, &g, &b);

            if (r < 10 && g < 10 && b < 10) 
	    {
                Coord p = {x, y};
                int added = 0;

                for (int i = 0; i < *blob_count && !added; i++) 
		{
                    for (int j = 0; j < blob_sizes[i]; j++) 
		    {
                        if (is_adjacent(p, res[i][j])) 
			{
                            blob_sizes[i]++;
                            res[i] = realloc(res[i], blob_sizes[i] * sizeof(Coord));
                            res[i][blob_sizes[i] - 1] = p;
                            added = 1;
                            break;
                        }
                    }
                }

                if (!added) 
		{
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

//foreach blob gives a corresponding Box, a Box being determined by the up-left-most
//and down-right-most pixels of a blob.
Box *compute_blob_boxes(Coord **blobs, int *blob_sizes, int blob_count) 
{
    if (!blobs || !blob_sizes || blob_count <= 0) return NULL;

    Box *boxes = malloc(blob_count * sizeof(Box));

    for (int i = 0; i < blob_count; i++) 
    {
        int minx = blobs[i][0].x;
        int maxx = blobs[i][0].x;
        int miny = blobs[i][0].y;
        int maxy = blobs[i][0].y;

        for (int j = 1; j < blob_sizes[i]; j++) 
	{
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

//Given a box, draw it on the master_surface
void draw_boxes(SDL_Surface *surface, Box *boxes, int count, Uint8 r, Uint8 g, Uint8 b) 
{
    if (!surface || !boxes || count <= 0) return;

    Uint32 color = SDL_MapRGB(surface->format, r, g, b);

    if (SDL_MUSTLOCK(surface))
        SDL_LockSurface(surface);

    Uint32 *pixels = (Uint32 *)surface->pixels;
    int w = surface->w;
    int stride = surface->pitch / 4;

    for (int i = 0; i < count; i++) 
    {
        Box box = boxes[i];

        for (int x = box.x; x < box.x + box.w; x++) 
	{
            if (x >= 0 && x < surface->w) 
	    {
                if (box.y >= 0 && box.y < surface->h)
                    pixels[box.y * stride + x] = color;
                if (box.y + box.h - 1 >= 0 && box.y + box.h - 1 < surface->h)
                    pixels[(box.y + box.h - 1) * stride + x] = color;
            }
        }

        for (int y = box.y; y < box.y + box.h; y++) 
	{
            if (y >= 0 && y < surface->h) 
	    {
                if (box.x >= 0 && box.x < surface->w)
                    pixels[y * stride + box.x] = color;
                if (box.x + box.w - 1 >= 0 && box.x + box.w - 1 < surface->w)
                    pixels[y * stride + (box.x + box.w - 1)] = color;
            }
        }
    }

    if (SDL_MUSTLOCK(surface))
        SDL_UnlockSurface(surface);
}

//Are a and b on the same row ? approximation based on value of max_y_diff.
//compared ycenter of box a and b if |centera.y - centerb.y| < max_diff then we consider
//they are likely on the same row (of course for reasonably low value of max_y_diff)
int same_row(Box a, Box b, int max_y_diff) 
{
    int ay = a.y + a.h / 2;
    int by = b.y + b.h / 2;
    return abs(ay - by) <= max_y_diff;
}

//Calls the differentiation for boxes algorithm and appends results to boxes array
Box* extract(Box* boxes, int n, int* out_n)
{
    if (!boxes || n <= 0 || !out_n) 
    {
        if (out_n) *out_n = 0;
        return NULL;
    }

    sort_boxes(boxes, (size_t)n);
    int new_count = 0;
    Box* new_boxes = differentiate_grid_list_and_words(boxes, n, &new_count);
    if (!new_boxes || new_count < 2) 
    {
        *out_n = n;
        return boxes;
    }

    int total_new = new_count;
    int total_old = n;
    int total_final = total_old + total_new;

    Box* extended = realloc(boxes, total_final * sizeof(Box));
    if (!extended) 
    {
        perror("realloc failed");
        free(new_boxes);
        *out_n = n;
        return boxes;
    }

    memcpy(&extended[total_old], new_boxes, total_new * sizeof(Box));
    *out_n = total_final;

    free(new_boxes);
    return extended;
}

//how far away on average are 2 subsequent boxes ? 
//(useful for approximating whether two boxes are part of a word, 
//of the grid
//or one is from a word and the other in the grid)
double compute_average_horizontal_gap(Box *boxes, int n, int max_y_diff) 
{
    if (n < 2) return 0.0;
    double total_gap = 0.0;
    int gap_count = 0;

    for (int i = 0; i < n - 1; i++) 
    {
        Box a = boxes[i];
        Box b = boxes[i + 1];

        if (same_row(a, b, max_y_diff)) 
	{
            int gap = b.x - (a.x + a.w);
            if (gap >= 0) 
	    {
                total_gap += gap;
                gap_count++;
            }
        }
    }
    if (gap_count == 0) return 0.0;
    return total_gap / gap_count;
}

//Sorts boxes by Y then by X (Previous attempt)
void sort_boxes(Box* boxes, size_t size)
{
    qsort(boxes, size, sizeof(Box), boxcmp);
}

//Compare doubles, for qsort
static int compare_double(const void *a, const void *b) 
{
    double diff = (*(double *)a - *(double *)b);
    return (diff > 0) - (diff < 0);
}

//compare boxes by Y then by X
static int compare_box_yx(const void *a, const void *b) 
{
    Box *A = (Box *)a, *B = (Box *)b;
    if (abs(A->y - B->y) > 10)
        return A->y - B->y;
    return A->x - B->x;
}

//Main algorithm to differentiate between the list, the grid and the words in the list
//using guesses about the disposition and way these parts are split.
//this assumes the list of words is to the side of the grid and wouldn't work on a
//differnet disposition, say the list is on top of the grid.
Box* differentiate_grid_list_and_words(Box* sorted, int n, int* out_count)
{
    if (n <= 0) {
        *out_count = 0;
        return NULL;
    }

    double *centers = malloc(n * sizeof(double));
    for (int i = 0; i < n; i++)
        centers[i] = sorted[i].x + sorted[i].w / 2.0;

    qsort(centers, n, sizeof(double), compare_double);
    int split_index = n - 1;
    double biggest_gap = 0.0;
    for (int i = 0; i < n - 1; i++) {
        double gap = centers[i + 1] - centers[i];
        if (gap > biggest_gap) {
            biggest_gap = gap;
            split_index = i;
        }
    }

    double split_x = (centers[split_index] + centers[split_index + 1]) / 2.0;
    free(centers);

    Box *left_boxes = malloc(n * sizeof(Box));
    Box *right_boxes = malloc(n * sizeof(Box));
    int left_count = 0, right_count = 0;

    for (int i = 0; i < n; i++) {
        double midx = sorted[i].x + sorted[i].w / 2.0;
        if (midx < split_x)
            left_boxes[left_count++] = sorted[i];
        else
            right_boxes[right_count++] = sorted[i];
    }

    Box *grid_boxes, *list_boxes;
    int gcount, lcount;
    if (left_count >= right_count) {
        grid_boxes = left_boxes; gcount = left_count;
        list_boxes = right_boxes; lcount = right_count;
    } else {
        grid_boxes = right_boxes; gcount = right_count;
        list_boxes = left_boxes; lcount = left_count;
    }

    Box grid = {INT_MAX, INT_MAX, 0, 0};
    Box list = {INT_MAX, INT_MAX, 0, 0};
    int gx2 = INT_MIN, gy2 = INT_MIN, lx2 = INT_MIN, ly2 = INT_MIN;

    for (int i = 0; i < gcount; i++) {
        Box b = grid_boxes[i];
        if (b.x < grid.x) grid.x = b.x;
        if (b.y < grid.y) grid.y = b.y;
        if (b.x + b.w > gx2) gx2 = b.x + b.w;
        if (b.y + b.h > gy2) gy2 = b.y + b.h;
    }
    grid.w = gx2 - grid.x;
    grid.h = gy2 - grid.y;

    for (int i = 0; i < lcount; i++) {
        Box b = list_boxes[i];
        if (b.x < list.x) list.x = b.x;
        if (b.y < list.y) list.y = b.y;
        if (b.x + b.w > lx2) lx2 = b.x + b.w;
        if (b.y + b.h > ly2) ly2 = b.y + b.h;
    }
    list.w = lx2 - list.x;
    list.h = ly2 - list.y;

    qsort(list_boxes, lcount, sizeof(Box), compare_box_yx);

    int *row_start = malloc(lcount * sizeof(int));
    int *row_end   = malloc(lcount * sizeof(int));
    int word_count = 0;

    if (lcount > 0) 
    {
        int start = 0;
        for (int i = 1; i < lcount; i++) 
	{
            int y1 = list_boxes[i - 1].y + list_boxes[i - 1].h / 2;
            int y2 = list_boxes[i].y + list_boxes[i].h / 2;
            if (abs(y2 - y1) > list_boxes[i].h * 0.8) 
	    {
                row_start[word_count] = start;
                row_end[word_count] = i - 1;
                word_count++;
                start = i;
            }
        }
        row_start[word_count] = start;
        row_end[word_count] = lcount - 1;
        word_count++;
    }

    int total = 2 + word_count;
    Box *result = malloc(total * sizeof(Box));

    result[0] = grid;
    result[1] = list;

    for (int w = 0; w < word_count; w++) 
    {
        int s = row_start[w];
        int e = row_end[w];
        Box wb = {INT_MAX, INT_MAX, 0, 0};
        int x2 = INT_MIN, y2 = INT_MIN;

        for (int i = s; i <= e; i++) 
	{
            Box b = list_boxes[i];
            if (b.x < wb.x) wb.x = b.x;
            if (b.y < wb.y) wb.y = b.y;
            if (b.x + b.w > x2) x2 = b.x + b.w;
            if (b.y + b.h > y2) y2 = b.y + b.h;
        }
        wb.w = x2 - wb.x;
        wb.h = y2 - wb.y;
        result[2 + w] = wb;
    }

    free(left_boxes);
    free(right_boxes);
    free(row_start);
    free(row_end);

    *out_count = total;
    return result;
}

//compare boxes a and b, for qsort (used in previous attempt), by Y then X
int boxcmp(const void* a, const void* b)
{

    const Box* boxa = (const Box*)a;
    const Box* boxb = (const Box*)b;
    if(boxa->y == boxb->y && boxa->x == boxb->x)
    {
	return 0;
    }
    if(boxa->y < boxb->y)
    {
	return -1;
    }
    if(boxa->y == boxb->y && boxa->x < boxb->x)
    {
	return -1;
    }
    return 1;
}
