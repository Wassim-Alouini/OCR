#pragma once

#include "preprocess.h"

typedef struct
{
    int inp_0;
    int h_1;
    int o_2;
    double lr;

    double** w_0_1;
    double** w_1_2;
    double* b_1;
    double* b_2;
} NeuralNetwork;

NeuralNetwork* init_nn(int inp, int h, int o, double lr);

void free_nn(NeuralNetwork* nn);

void forward(NeuralNetwork* nn, const double* input, double* hidden,
             double* output);

void SGD(NeuralNetwork* nn, Dataset* ds, int epochs, double learning_rate);

void nn_save(NeuralNetwork* nn, const char* filename);

NeuralNetwork* nn_load(const char* filename);

char nn_predict_letter(const char* model_file, const char* image_file);
