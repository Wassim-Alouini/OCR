#include <stdio.h>
#include "neuralnetwork.h"

int main()
{
	
	double inputs[4][2] = 
	{
		{0, 0},
		{0, 1},
		{1, 0},
		{1, 1}
	};
	
	double labels[4][1] = 
	{
		{1},
		{0},
		{0},
		{1}
	};
	
	NeuralNetwork *nn = init_nn(2, 2, 1, 0.5);
	
	SGD(nn, inputs, labels, 4, 100000);
	
	
	printf("\nfinal output\n");
	
	for (int i = 0; i < 4; i++)
	{
		double *output = forward(nn, inputs[i]);
		printf("Input: %.0f %.0f    Output: %f   (Label: %.0f)\n",inputs[i][0], inputs[i][1], output[0], labels[i][0]);
	}
	
	free_nn(nn);
	return 0;
}

