#include "preprocess.h"
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// utility fonction to nornalise the size of images to 24*24
// nearest neighbor algorithm
void resize_nearest_gray(const uint8_t* src, int src_w, int src_h, uint8_t* dst,
                         int dst_w, int dst_h)
{
    if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0)
    {
        errx(EXIT_FAILURE, "resize_nearest_gray() invalid gray size\n");
    }
    for (int y = 0; y < dst_h; ++y)
    {
        int src_y = y * src_h / dst_h;
        if (src_y >= src_h)
            src_y = src_h - 1;
        for (int x = 0; x < dst_w; ++x)
        {
            int src_x = x * src_w / dst_w;
            if (src_x >= src_w)
                src_x = src_w - 1;
            dst[y * dst_w + x] = src[src_y * src_w + src_x];
        }
    }
}

// take a BMP file and produce a vector of double
// 1.0 (black pixel) or 0.0 (white pixel)
// this vector will be the input of the nn of size 24*24
void load_bmp_as_vector(const char* filename, int target_w, int target_h,
                        double* out_vec)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        errx(EXIT_FAILURE, "load_bmp_as_vector() failed to open file %s\n",
             filename);
    }
    // reading header informations to get size type and and compatiblility
    // of the file
    uint16_t bfType;

    if (fread(&bfType, sizeof(uint16_t), 1, f) != 1)
    {
        fclose(f);
        errx(EXIT_FAILURE, "load_bmp_as_vector() file read error\n");
    }
    if (bfType != 0x4D42)
    {
        fclose(f);
        errx(EXIT_FAILURE,
             "load_bmp_as_vector() file format error : %s is not a bmp file\n",
             filename);
    }

    fseek(f, 8, SEEK_CUR);

    uint32_t bfOffBits;

    if (fread(&bfOffBits, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        errx(EXIT_FAILURE, "load_bmp_as_vector() file read error\n");
    }

    uint32_t biSize;

    if (fread(&biSize, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        errx(EXIT_FAILURE, "load_bmp_as_vector() file read error\n");
    }

    if (biSize < 40)
    {
        fclose(f);
        errx(EXIT_FAILURE, "Unsupported bmp header size (%u) in %s\n", biSize,
             filename);
    }

    int32_t biWidth, biHeight;
    uint16_t biPlanes, biBitCount;
    uint32_t biCompression, biSizeImage;
    int32_t biXPelsPerMeter, biYPelsPerMeter;
    uint32_t biClrUsed, biClrImportant;

    if (fread(&biWidth, sizeof(int32_t), 1, f) != 1 ||
        fread(&biHeight, sizeof(int32_t), 1, f) != 1 ||
        fread(&biPlanes, sizeof(uint16_t), 1, f) != 1 ||
        fread(&biBitCount, sizeof(uint16_t), 1, f) != 1 ||
        fread(&biCompression, sizeof(uint32_t), 1, f) != 1 ||
        fread(&biSizeImage, sizeof(uint32_t), 1, f) != 1 ||
        fread(&biXPelsPerMeter, sizeof(int32_t), 1, f) != 1 ||
        fread(&biYPelsPerMeter, sizeof(int32_t), 1, f) != 1 ||
        fread(&biClrUsed, sizeof(uint32_t), 1, f) != 1 ||
        fread(&biClrImportant, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        errx(EXIT_FAILURE, "load_bmp_as_vector() file read error\n");
    }
    if (!((biBitCount == 8 || biBitCount == 24) && biCompression == 0) &&
        !(biBitCount == 32 && (biCompression == 0 || biCompression == 3)))
    {
        fclose(f);
        errx(EXIT_FAILURE,
             "Unsupported BMP format (%s): bpp=%d, compression=%u\n", filename,
             biBitCount, biCompression);
    }

    int width = biWidth;
    int height = biHeight;
    int flip_y = 0;

    if (height < 0)
    {
        height = -height;
        flip_y = 0;
    }
    else
    {
        flip_y = 1;
    }

    if (biBitCount != 24 && biBitCount != 8 && biBitCount != 32)
    {
        fclose(f);
        errx(EXIT_FAILURE,
             "Only 8, 24 or 32-bit BMP supported (%s has %d bpp)\n", filename,
             biBitCount);
    }

    fseek(f, bfOffBits, SEEK_SET);

    int row_padded = ((biBitCount * width + 31) / 32) * 4;

    uint8_t* gray = malloc(width * height * sizeof(*gray));

    uint8_t* row = malloc(row_padded * sizeof(*row));

    // reading the file to fill the *gray vector
    for (int y = 0; y < height; ++y)
    {
        if (fread(row, 1, row_padded, f) != (size_t)row_padded)
        {
            free(gray);
            free(row);
            fclose(f);
            errx(EXIT_FAILURE,
                 "load_bmp_as_vector() Error reading pixel data from %s\n",
                 filename);
        }
        int dst_y = flip_y ? (height - 1 - y) : y;
        for (int x = 0; x < width; ++x)
        {
            if (biBitCount == 24)
            {
                int idx = x * 3;
                uint8_t B = row[idx + 0];
                uint8_t G = row[idx + 1];
                uint8_t R = row[idx + 2];
                uint8_t g = (uint8_t)((R + G + B) / 3);
                gray[dst_y * width + x] = g;
            }
            else if (biBitCount == 8)
            {
                gray[dst_y * width + x] = row[x];
            }
            else if (biBitCount == 32)
            {
                int idx = x * 4;
                uint8_t B = row[idx + 0];
                uint8_t G = row[idx + 1];
                uint8_t R = row[idx + 2];
                uint8_t g = (uint8_t)((R + G + B) / 3);
                gray[dst_y * width + x] = g;
            }
        }
    }
    free(row);
    fclose(f);

    // Resizing the vector with the resize_nearest_gray(6) function
    uint8_t* resized = malloc(target_w * target_h * sizeof(*resized));

    resize_nearest_gray(gray, width, height, resized, target_w, target_h);

    free(gray);

    // Binarize with treshold 128
    for (int i = 0; i < target_w * target_h; ++i)
    {
        uint8_t g = resized[i];
        out_vec[i] = (g < 128) ? 1.0 : 0.0;
    }

    free(resized);
}

// Dataset functions

void dataset_free(Dataset* ds)
{

    if (ds->inputs)
    {
        for (int i = 0; i < ds->num_samples; ++i)
        {
            free(ds->inputs[i]);
        }
        free(ds->inputs);
    }

    free(ds->labels);
    free(ds);
}

// reading the train.txt file and return a struct Dataset filled with :
// vector inputs
// labels
// num of samples (input,label)
Dataset* dataset_load(const char* list_file)
{
    FILE* f = fopen(list_file, "r");
    if (!f)
    {
        errx(EXIT_FAILURE, "dataset_load() failed to open %s\n", list_file);
    }

    int n;

    if (fscanf(f, "%d", &n) != 1 || n <= 0)
    {
        fclose(f);
        errx(EXIT_FAILURE, "Invalid dataset file header\n");
    }

    Dataset* ds = malloc(sizeof(Dataset));

    ds->num_samples = n;
    ds->inputs = malloc(sizeof(double*) * n);
    ds->labels = malloc(sizeof(int) * n);

    for (int i = 0; i < n; ++i)
    {
        char label_char;
        char path[512];

        if (fscanf(f, " %c %511s", &label_char, path) != 2)
        {
            dataset_free(ds);
            fclose(f);
            errx(EXIT_FAILURE, "Error reading dataset line %d\n", i + 1);
        }

        int label = label_char - 'A';
        ds->labels[i] = label;

        ds->inputs[i] = malloc(sizeof(double) * INPUT_SIZE);

        load_bmp_as_vector(path, IMG_WIDTH, IMG_HEIGHT, ds->inputs[i]);
    }
    fclose(f);
    return ds;
}
