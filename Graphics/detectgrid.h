#pragma once
#include "bounds.h"

int box_height_cmp ( const void * first, const void * second );
int middle_cmp(const void *a, const void *b);
Box * sort_box(Box* box,int boxCount);
double calculate_quartile(Box *box, int n, double position);
double iqr(Box* sortedBox,int boxCount,double* Q1, double* Q3);
double calculate_surface_mediane(Box* sortedBox, int boxCount);
double calculate_height_mediane(Box* sortedBox, int boxCount);
double calculate_width_mediane(Box* sortedBox, int boxCount);
Box* Find_Letters(Box* box,int boxCount,int* newCount);
Box* organised_letter_box(Box* box, Box* letterBox, int BoxCount, int letterBoxCount);
void middle_box(Box box,int *x,int *y);
float  calculate_distance(int x1, int y1, int x2,int y2);
void separate_grid_word(Box* boxes, int boxCount, Box*** gridBoxes,int** gridCount, Box*** wordBoxes, int** wordCount,int* nbLinesGrid, int* nbLinesWord);
void draw_word(SDL_Renderer *renderer, Box ***boxes,int x1, int y1, int x2, int y2);
void fillCircleAlpha(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color);
void separate_grid_word_v2(Box *boxes, int boxCount,
                           Box ***gridBoxes, int **gridCount,
                           Box ***wordBoxes, int **wordCount,
                           int *nbLinesGrid, int *nbLinesWord,
                           float medianHeight);
int should_use_separate_v2(Box *boxes, int boxCount, float medianHeight);
void draw_word_surface(SDL_Surface **surface, Box ***boxes, int x1, int y1, int x2, int y2);
