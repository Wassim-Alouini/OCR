#pragma once

typedef struct
{
	int inp_0;
	int h_1;
	int o_2;
	double lr;
	
	double **w_0_1;
	double **w_1_2;
	double *b_1;
	double *b_2;

} NeuralNetwork;

NeuralNetwork *init_nn(int inp, int h, int o, double lr);

void SGD(NeuralNetwork *nn, double inputs[][2], double labels[][1], int data, int epochs);

double *forward(NeuralNetwork *nn, double *input);

void free_nn(NeuralNetwork *nn);	

