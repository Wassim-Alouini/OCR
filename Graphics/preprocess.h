#pragma once

#include "config.h"

typedef struct
{
    int num_samples;
    double** inputs;
    int* labels;
} Dataset;

void load_bmp_as_vector(const char* filename, int target_w, int target_h,
                        double* out_vec);

Dataset* dataset_load(const char* list_file);

void dataset_free(Dataset* ds);
