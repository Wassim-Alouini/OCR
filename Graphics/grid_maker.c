#include <unistd.h>
#include <stdio.h>
#include "solver.h"




char *line_maker(int lineCount,const char* folder,char* modele)
{
	char* res = malloc((lineCount + 1) * sizeof(char));
	if(!res)
	{
		fprintf(stderr, "line_maker: Error malloc\n");
		return NULL;
	}

	for(int i = 0; i < lineCount; i++)
	{
		char filename[256];
        	snprintf(filename, sizeof(filename), "%s/_box_%d.bmp", folder, i);
		char letter = fonction_tom(filename,modele);
		res[i] = letter;
	}

	res[lineCount] = 0;

	return res;
}



void grid_maker(Box*** gridLines,int **gridCount,
		int nbLinesGrid,SDL_Surface** master_surface, char* modele)
{
	for(int i = 0; i < nbLinesGrid; i++)
	{
		extract_boxes_to_bmp(*master_surface,(*gridLines)[i],(*gridCount)[i], "images");
		char* line = line_maker((*gridCount)[i],"images",modele);
		if(!line)
		{
			return;
		}

                char filename[256];

                for (int j = 0; j < (*gridCount)[i]; j++)
                {
                        snprintf(filename,sizeof(filename),"%s/_box_%d.bmp","images",j);
                        if (remove(filename) != 0)
                        {
                                return;
                        }
                }

		FILE *f = fopen("grid.txt", "a");
    		if (!f)
		{
			free(line);	
        		return;
		}

    		fprintf(f, "%s\n",line);

    		fclose(f);
		free(line);
	}
	
}

char** word_maker(Box*** wordLines,int **wordCount,int nbLinesWord,SDL_Surface** master_surface,char* modele)
{
	char ** wordList = malloc(nbLinesWord * sizeof(char*));
	if(!wordList)
		return NULL;

        for(int i = 0; i < nbLinesWord; i++)
        {
                extract_boxes_to_bmp(*master_surface,(*wordLines)[i],(*wordCount)[i], "images");
                char* line = line_maker((*wordCount)[i],"images",modele);
                if(!line)
		{
			for (int k = 0; k < i; k++)
                	{
				free(wordList[k]);
			}

		    	free(wordList);
            		return NULL;
		}
		
		char filename[256];

    		for (int j = 0; j < (*wordCount)[i]; j++)
    		{
		        snprintf(filename,sizeof(filename),"%s/_box_%d.bmp","images",j);
	
        		if (remove(filename) != 0)
        		{
				for (int k = 0; k < i; k++)
                        	{
                                	free(wordList[k]);
                        	}

                        	free(wordList);
                        	return NULL;
        		}
    		}

        	wordList[i] = line;        
        }


	return wordList;
}


void solver_call(char **wordlist, int nbLinesWord,int* x1, int* y1, int* x2,int* y2)
{
	for(int i = 0; i < nbLinesWord; i++)
	{
		solver("grid.txt",wordlist[i],x1,y1,x2,y2);
	}
}


int main()
{
	grid_maker();
	char** wordlist = word_maker();
	solver_call();
	draw_word();

}
