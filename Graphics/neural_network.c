#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <err.h>
#include "neural_network.h"
#include "config.h"
#include "preprocess.h"

//sigmoid function used for the forward
//max absol
static double sigm(double x)
{
	if (x < -30.0)
	{
		return 1e-13;
	}
	if (x > 30.0)
	{
		return 1.0 - 1e-13;
	}
	
	return 1.0/(1.0 + exp(-x));
}


//sigmoid prime used in the backpropagate
static double sigm_prime(double x)
{
	return x * (1.0 - x);
}


//init bias and weignt with rand deterministic values
//using sin() and w indices
static double rand_w(int i, int j)
{
	double val = sin((i * 37 + j * 17 + 1) * 0.5);
	return val;
}

//allocate a matrix to store the weight 
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

//initialise a NeuralNetwork struct
NeuralNetwork *init_nn(int inp, int h, int o, double lr)
{	
	NeuralNetwork *nn = malloc(sizeof(NeuralNetwork));
	nn->inp_0 = inp;
	nn->h_1 = h;
	nn->o_2 = o;
	nn->lr = lr;

	nn->w_0_1 = init_m(inp, h);
	nn->w_1_2 = init_m(h, o);
	nn->b_1 = malloc(h * sizeof(double));
	nn->b_2 = malloc(o * sizeof(double));


	for (int i = 0; i < inp; ++i)
	{
		for (int j = 0; j < h; ++j)
		{
			nn->w_0_1[i][j] = rand_w(i, j);
		}
	}
	for (int j = 0; j < h; ++j)
	{
		nn->b_1[j] = rand_w(j + 20, j + 20);
	}



	for (int j = 0; j < h; ++j)
	{
		for (int k = 0; k < o; ++k)
		{
			nn->w_1_2[j][k] = rand_w(j + 10, k + 10);
		}
	}
	for (int k = 0; k < o; ++k)
	{
		nn->b_2[k] = rand_w(k + 30, k + 30);
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

//compute the output layer from the input given in parameter
void forward(NeuralNetwork *nn, const double *input, double *hidden, double *output)
{

	int inp = nn->inp_0;
	int h = nn->h_1;
	int out = nn->o_2;


	for (int j = 0; j < h; ++j)
	{
		double z = nn->b_1[j];
		for (int i = 0; i < inp; ++i)
		{
			z += input[i] * nn->w_0_1[i][j];
		}
		hidden[j] = sigm(z);
	}


	double *out_val = malloc(sizeof(double) * out);

	for (int k = 0; k < out; ++k)
	{
		double z = nn->b_2[k];

		for (int j = 0; j < h; ++j)
		{
			z += hidden[j] * nn->w_1_2[j][k];
		}

		out_val[k] = z;
	}

	//softmax to convert output values to probabilities
	double max_val = out_val[0];
	for (int k = 1; k < out; ++k)
	{
		if (out_val[k] > max_val)
		{
			max_val = out_val[k];
		}
	}

	double sum_exp = 0.0;
	for (int k = 0; k < out; ++k)
	{
		output[k] = exp(out_val[k] - max_val);
		sum_exp += output[k];
	}
	for (int k = 0; k < out; ++k)
	{
		output[k] /= sum_exp;
	}

	free(out_val);
}

//training loop 
void SGD(NeuralNetwork *nn, Dataset *ds, int epochs, double lr)
{
	int inp = nn->inp_0;
	int h = nn->h_1;
	int out = nn->o_2;

	double *h_1 = malloc(sizeof(double) * h);
	double *o_2 = malloc(sizeof(double) * out);
	double *grad_h_1 = malloc(sizeof(double) * h);
	double *grad_o_2 = malloc(sizeof(double) * out);

	for (int epoch = 0; epoch < epochs; ++epoch)
	{
		double total_loss = 0.0;

		for (int n = 0; n < ds->num_samples; ++n)
		{
			double *input = ds->inputs[n];
			int label = ds->labels[n];

			//fill the o_2 vector with the result of the forward
			forward(nn, input, h_1, o_2);

			// cross-entropy to calculate the loss
			double eps = 1e-15;
			double prob = o_2[label];
			if (prob < eps)
				prob = eps;
			total_loss += -log(prob);


			//backpropagate : compute the gradient for each layer 
			//and update the weight and biais according to the lr
			for (int k = 0; k < out; ++k)
			{
				double y = (k == label) ? 1.0 : 0.0;
				grad_o_2[k] = o_2[k] - y;
			}

			for (int j = 0; j < h; ++j)
			{
				double sum = 0.0;
				for (int k = 0; k < out; ++k)
				{
					sum += nn->w_1_2[j][k] * grad_o_2[k];
				}
				grad_h_1[j] = sum * h_1[j] * (1.0 - h_1[j]);
			}

			for (int j = 0; j < h; ++j)
			{
				for (int k = 0; k < out; ++k)
				{
					nn->w_1_2[j][k] -= lr * grad_o_2[k] * h_1[j];
				}
			}
			for (int k = 0; k < out; ++k)
			{
				nn->b_2[k] -= lr * grad_o_2[k];
			}


			for (int i = 0; i < inp; ++i)
			{
				for (int j = 0; j < h; ++j)
				{
					nn->w_0_1[i][j] -= lr * grad_h_1[j] * input[i];
				}
			}
			for (int j = 0; j < h; ++j)
			{
				nn->b_1[j] -= lr * grad_h_1[j];
			}
		}

		double moy_loss = total_loss / ds->num_samples;

		printf("Epoch %d - loss = %f\n", epoch + 1, moy_loss);
	}

	free(h_1);
	free(o_2);
	free(grad_h_1);
	free(grad_o_2);
}

//fill a .txt file with all the bias and weight from a nn given in parameter
void nn_save(NeuralNetwork *nn, const char *filename)
{
	FILE *f = fopen(filename, "w");

	if (!f)
	{
		errx(EXIT_FAILURE, "nn_save() : open file model\n");
	}

	fprintf(f, "%d %d %d\n", nn->inp_0, nn->h_1, nn->o_2);

	for (int j = 0; j < nn->h_1; ++j)
	{
		fprintf(f, "%.15f ", nn->b_1[j]);
	}
	fprintf(f, "\n");

	for (int i = 0; i < nn->inp_0; ++i)
	{
		for (int j = 0; j < nn->h_1; ++j)
		{
			fprintf(f, "%.15f ", nn->w_0_1[i][j]);
		}
	}
	fprintf(f, "\n");

	for (int k = 0; k < nn->o_2; ++k)
	{
		fprintf(f, "%.15f ", nn->b_2[k]);
	}
	fprintf(f, "\n");

	for (int j = 0; j < nn->h_1; ++j)
	{
		for (int k = 0; k < nn->o_2; ++k)
		{
			fprintf(f, "%.15f ", nn->w_1_2[j][k]);
		}
	}
	fprintf(f, "\n");

	fclose(f);
}
//gereate a nn struct and fill the matirx of weight and bias 
//with the content of the .txt file given in parameter
NeuralNetwork *nn_load(const char *filename)
{
	FILE *f = fopen(filename, "r");
	if (!f)
	{
		errx(EXIT_FAILURE, "nn_load : open file model\n");
	}

	int inp;
       	int h; 
	int out;


	if (fscanf(f, "%d %d %d", &inp, &h, &out) != 3)
	{
		fclose(f);
		errx(EXIT_FAILURE,"invalid model header\n");
	}

	NeuralNetwork *nn = init_nn(inp, h, out, 0.0);

	for (int j = 0; j < nn->h_1; ++j)
	{
		fscanf(f, "%lf", &nn->b_1[j]);

	}

	for (int i = 0; i < nn->inp_0; ++i)
	{
		for (int j = 0; j < nn->h_1; ++j)
		{
			fscanf(f, "%lf", &nn->w_0_1[i][j]);
		}
	}

	for (int k = 0; k < nn->o_2; ++k)
	{
		fscanf(f, "%lf", &nn->b_2[k]);
	}

	for (int j = 0; j < nn->h_1; ++j)
	{
		for (int k = 0; k < nn->o_2; ++k)
		{
			fscanf(f, "%lf", &nn->w_1_2[j][k]);
		}
	}

	fclose(f);
	return nn;
}



//main function that will be called by the project when executed
//return prediction char from a path to a file .bmp 
//using the model.txt saved
char nn_predict_letter(const char *model_file, const char *image_file)
{
	if (!model_file || !image_file)
	{
		errx(EXIT_FAILURE,"nn_perdict_letter : parameter error\n");
	}
	//initialise the nn that will make the prediction
	NeuralNetwork *nn = nn_load(model_file);


	double *inp_0 = malloc(sizeof(double) * INPUT_SIZE);

	//compute the input vector form the image file given
	//using the preprocess.c file
	load_bmp_as_vector(image_file, IMG_WIDTH, IMG_HEIGHT, inp_0);

	double *h_1 = malloc(sizeof(double) * nn->h_1);
	double *o_2 = malloc(sizeof(double) * nn->o_2);

	//use the nn to predict the letter 
	forward(nn, inp_0, h_1, o_2);

	int result = 0;

	double max_val = o_2[0];
	for(int i = 1; i < nn->o_2; i++)
	{
		if(o_2[i] > max_val)
		{
			max_val = o_2[i];
			result = i;
		}
	}
	
	free(inp_0);
	free(h_1);
	free(o_2);
	free_nn(nn);
	//return the character predicted to the project
	return (char)('A' + result);
}

