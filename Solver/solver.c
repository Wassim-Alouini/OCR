#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Turns every letter of the word into uppercase
void strtoupper(char *word)
{
    int len = strlen(word);
    for (int i = 0; i < len; i++)
    {
        word[i] = toupper(word[i]);
    }
}

// Main algorithm to search a word inside the grid
int algoSolver(int rows,int cols,char tab[rows][cols],char* word)
{
    int len = strlen(word);
    strtoupper(word); // Convert the word to uppercase

    // All 8 possible directions (horizontal, vertical and diagonal)
    int directions[8][2] = {
        {0,1},
        {1,1},
        {1,0},
        {1,-1},
        {0,-1},
        {-1,-1},
        {-1,0},
        {-1,1}
    };

    // Go through the whole grid
    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < cols; j++)
        {
            // If the current cell matches the first letter of the word
            if(tab[i][j] == word[0])
            {
                // Try all 8 directions
                for(int k = 0; k < 8 ; k++)
                {
                    int indexI = i;
                    int indexJ = j;
                    int indexW = 0;
                    int di = directions[k][0];
                    int dj = directions[k][1];

                    // Check if the word can actually fit in this direction
                    if((i + di * (len - 1) < 0) || (i + di * (len - 1) >= rows)
                        || (j + dj * (len - 1) < 0) || (j + dj * (len - 1) >= cols))
                    {
                        continue;
                    }

                    // Move through the grid while letters are matching
                    while(indexW < len && indexI >= 0 && indexI < rows
                        && indexJ >= 0 && indexJ < cols
                        && tab[indexI][indexJ] == word[indexW])
                    {
                        indexI += di;
                        indexJ += dj;
                        indexW++;
                    }

                    // If we reached the end of the word, it means it was found
                    if(indexW == len)
                    {
                        printf("(%i,%i) (%i,%i)\n",i,j,indexI-di,indexJ-dj);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

// Reads the grid from the file and launches the search
void solver(char* filename,char* word)
{
    FILE *file = fopen(filename,"r");

    char* line = malloc(10000 * sizeof(char)); // buffer to store the grid content
    int rows = 0;
    int nbchar = 0;
    int index = 0;
    int c;

    // Read the file character by character to count rows and columns
    while((c = fgetc(file)) != EOF)
    {
        if(c != '\n')
        {
            line[index++] = c;
            nbchar++;
        }
        else
        {
            rows++;
        }
    }
    fclose(file);

    line[index] = 0;
    rows++; // last line isn’t counted in the loop

    int cols = nbchar / rows; // number of columns in the grid

    // Fill the 2D array with characters from the line
    char tab[rows][cols];
    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < cols; j++)
        {
            tab[i][j] = line[i * cols + j];
        }
    }

    free(line);

    // If the word wasn’t found, print a message
    if(!algoSolver(rows,cols,tab,word))
    {
        printf("Not Found\n");
    }
}

// Main function
int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        printf("Usage: %s <grid_file> <word>\n", argv[0]);
        return 0;
    }

    printf("\nWord: %s\n", argv[2]);
    solver(argv[1],argv[2]);

    return 0;
}
