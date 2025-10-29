#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "neuralnetwork.h"

//utilitary function
//
static double sigm(double x)
{
	return 1.0 / (1.0 + exp(-x));
}

static double sigm_prime(double x)
{
	return x * (1.0 - x);
}


static double **init_m(int rows, int cols)
{
	double **m = malloc(rows * sizeof(double *));
	for (int i = 0; i < rows; i++)
	{
		m[i] = malloc(cols * sizeof(double));
	}
	return m;
}



static void free_m(double **m, int rows)
{
	for (int i = 0; i < rows; i++)
	{
		free(m[i]);
	}
	free(m);

}
//generation of deterministic random values for the weight and bias init
static double rand_w(int i, int j)
{
	double val = sin((i * 37 + j * 17 + 1) * 0.5);
	return val;
}


NeuralNetwork *init_nn(int inp, int h, int o, double lr)
{
	NeuralNetwork *nn = malloc(sizeof(NeuralNetwork));
	nn->inp_0 = inp;
	nn->h_1 = h;
	nn->o_2 = o;
	nn->lr = lr;
	//dynamic allocation of weight and bias
	nn->w_0_1 = init_m(inp, h);
	nn->w_1_2 = init_m(h, o);       
	nn->b_1 = malloc(h * sizeof(double));
	nn->b_2 = malloc(o * sizeof(double));
	//init of weight ans bias 
	for (int i = 0; i < inp; i++)
	{
		for (int j = 0; j < h; j++)
		{
			nn->w_0_1[i][j] = rand_w(i, j);
		}
	}
	
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < o; j++)
		{
			nn->w_1_2[i][j] = rand_w(i + 10, j + 10);
		}
	}
	
	for (int j = 0; j < h; j++)
	{
		nn->b_1[j] = rand_w(j + 100, j);
	}
	
	for (int j = 0; j < o; j++)
	{
		nn->b_2[j] = rand_w(j + 200, j);
	}
	
	return nn;
}

void free_nn(NeuralNetwork *nn)
{
	free_m(nn->w_0_1, nn->inp_0);
	free_m(nn->w_1_2, nn->h_1);
	free(nn->b_1);
	free(nn->b_2);
	free(nn);
}

//compute the output of the nn depending on the input data given
double *forward(NeuralNetwork *nn, double *input) 
{
	double *h_1 = malloc(nn->h_1 * sizeof(double));
	double *o_2 = malloc(nn->o_2 * sizeof(double));
	
	for (int j = 0; j < nn->h_1; j++)
	{
		double z = nn->b_1[j];
		for (int i = 0; i < nn->inp_0; i++)
		{
			z += input[i] * nn->w_0_1[i][j];
		}
		h_1[j] = sigm(z);
	}
	for (int k = 0; k < nn->o_2; k++) 
	{
		double z = nn->b_2[k];
		for (int j = 0; j < nn->h_1; j++)
		{
			z += h_1[j] * nn->w_1_2[j][k];
			o_2[k] = sigm(z);
		}
	}
	free(h_1);
	return o_2;
}

//backprop: 1 epoch = 4 backprop : 1 for each data input possible
void SGD(NeuralNetwork *nn, double inputs[][2], double labels[][1], int data, int epochs) 
{
	for (int epoch = 0; epoch < epochs; epoch++)
	{
		double total_loss = 0.0;  
		
		for (int s = 0; s < data; s++) 
		{
			//compute the total loss of the forward 
			double *h_1 = malloc(nn->h_1 * sizeof(double));
			double *o_2 = malloc(nn->o_2 * sizeof(double));
			
			for (int j = 0; j < nn->h_1; j++) 
			{
				double z = nn->b_1[j];
				
				for (int i = 0; i < nn->inp_0; i++)
				{
					z += inputs[s][i] * nn->w_0_1[i][j];
					h_1[j] = sigm(z);
				}
			
			}
			for (int k = 0; k < nn->o_2; k++) 
			{
				double z = nn->b_2[k];
				for (int j = 0; j < nn->h_1; j++)
				{
					z += h_1[j] * nn->w_1_2[j][k];
					o_2[k] = sigm(z);
				}
			}
			
			double error[1];
			
			for (int k = 0; k < nn->o_2; k++)
			{
				error[k] = labels[s][k] - o_2[k];
				total_loss += error[k] * error[k];
			}
			
			//compute the local loss of each neuron
			double grad_o_2[1];
			
			for (int k = 0; k < nn->o_2; k++)
			{
				grad_o_2[k] = error[k] * sigm_prime(o_2[k]);
			}
			
			double *grad_h_1 = malloc(nn->h_1 * sizeof(double));
			
			for (int j = 0; j < nn->h_1; j++)
			{
				double err = 0.0;
				
				for (int k = 0; k < nn->o_2; k++)
				{
					err += grad_o_2[k] * nn->w_1_2[j][k];
					grad_h_1[j] = err * sigm_prime(h_1[j]);
				}
			}
			//update the weight depending on local loss and learning rate
			for (int j = 0; j < nn->h_1; j++)
			{
				for (int k = 0; k < nn->o_2; k++) 
				{
					nn->w_1_2[j][k] += nn->lr * grad_o_2[k] * h_1[j];
				}
			}
			for (int i = 0; i < nn->inp_0; i++)
			{
				for (int j = 0; j < nn->h_1; j++) 
				{
					nn->w_0_1[i][j] += nn->lr * grad_h_1[j] * inputs[s][i];
				}
			}
			
			for (int j = 0; j < nn->h_1; j++)
			{
				nn->b_1[j] += nn->lr * grad_h_1[j];
			}
			for (int k = 0; k < nn->o_2; k++)
			{
				nn->b_2[k] += nn->lr * grad_o_2[k];
			}
			
			free(h_1);
			free(o_2);
			free(grad_h_1);
		}
		//follow the convergence each 1000 iterations 
		if (epoch % 1000 == 0) printf("Epoch %d   Total Loss: %.6f\n", epoch, total_loss);
	}





}
