#pragma once

void strtoupper(char *word);
int algoSolver(int rows,int cols,char tab[rows][cols],char* word,
		int* x1, int* y1, int* x2,int* y2);
void solver(char* filename,char* word,int* x1, int* y1, int* x2,int* y2);
