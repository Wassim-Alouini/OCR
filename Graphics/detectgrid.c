#include "detectgrid.h"

/*Box* Init(SDL_Surface* surface,int* boxCount)
{
    int blob_count = 0;
    int* blob_sizes = NULL;

    Coord** blobs = find_blobs_rec(master_surface, &blob_count, &blob_sizes);
    
    printf("Found %d blobs\n", blob_count);

    Box *boxes = compute_blob_boxes(blobs, blob_sizes, blob_count);
    boxCount = blob_count;

    return boxes;
}*/

int box_height_cmp ( const void * first, const void * second ) 
{
    const Box * firstBox =  (const Box *) first;
    const Box * secondBox =  (const Box *) second;
    return (firstBox->h) - (secondBox->h);
}

/*int middle_cmp ( const void * first, const void * second ) 
{
    float * a =  (const float *) first;
    float * b =  (const float *) second;
    return (*a) - (*b);
}*/

int middle_cmp(const void *a, const void *b)
{
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

Box * sort_box(Box* box,int boxCount)
{
    Box* newBox = malloc(boxCount * sizeof(Box));
    memmove(newBox,box,boxCount * sizeof(Box));

    qsort(newBox,boxCount,sizeof(Box),box_height_cmp);

    return newBox;
}

double calculate_quartile(Box *box, int n, double position) 
{
    int index = floor(position);
    double fraction = position - index;

    if (fraction == 0) 
    {
        return box[index - 1].h * box[index - 1].w;
    } 
    else 
    {
        return (box[index - 1].h * box[index - 1].w) * (1 - fraction) 
		+ (box[index].h * box[index].w) * fraction;
    }
}


double iqr(Box* sortedBox,int boxCount,double* Q1, double* Q3)
{
    double q1_pos = 0.25 * (boxCount + 1);
    double q3_pos = 0.75 * (boxCount + 1);

    *Q1 = calculate_quartile(sortedBox,boxCount,q1_pos);
    *Q3 = calculate_quartile(sortedBox,boxCount,q3_pos);

    double IQR = *Q3 - *Q1;

    printf("IQR : %f\n",IQR);
    return IQR;
}

double calculate_surface_mediane(Box* sortedBox, int boxCount)
{
    double mediane = 0;
    int middle = boxCount / 2;

    if(boxCount % 2 == 0)
    {
	double surface1 = sortedBox[middle].h * sortedBox[middle].w;
	double surface2 = sortedBox[middle - 1].h * sortedBox[middle -1].w;
	mediane = (surface1 + surface2) / 2;
	
	return mediane;
    }

    mediane = sortedBox[middle].h * sortedBox[middle].w;
    return mediane;
}

double calculate_height_mediane(Box* sortedBox, int boxCount)
{
    double mediane = 0;
    int middle = boxCount / 2;

    if(boxCount % 2 == 0)
    {
	double height1 = sortedBox[middle].h;
	double height2 = sortedBox[middle - 1].h;
	mediane = (height1 + height2) / 2;
	
	return mediane;
    }

    mediane = sortedBox[middle].h;
    return mediane;
}

double calculate_width_mediane(Box* sortedBox, int boxCount)
{
    double mediane = 0;
    int middle = boxCount / 2;

    if(boxCount % 2 == 0)
    {
	double width1 = sortedBox[middle].w;
	double width2 = sortedBox[middle - 1].w;
	mediane = (width1 + width2) / 2;
	
	return mediane;
    }

    mediane = sortedBox[middle].w;
    return mediane;
}


Box* Find_Letters(Box* box,int boxCount,int* newCount)
{
    Box *newBox = sort_box(box,boxCount);

    //tri par rapport a la height
    double mediane = calculate_height_mediane(newBox,boxCount);
    double boundMin = mediane * 0.9;
    double boundMax = mediane * 2.5;

    int count = 0;
    for(int i = 0; i < boxCount; i++)
    {
	if(newBox[i].h  < boundMin || newBox[i].h > boundMax)
	{
		newBox[i].h = 0;
		newBox[i].w = 0;
		count++;
	}

    }

    printf("count: %i\n",count);
    Box *resBox = malloc((boxCount - count) * sizeof(Box));

    Box *sortedNew = sort_box(newBox,boxCount);

    for(int j = count; j < boxCount; j++)
    {
	if(sortedNew[j].h != 0 && sortedNew[j].w != 0)
	{
		resBox[j - count] = sortedNew[j];
	}
    }

    free(newBox);
    free(sortedNew);

    *newCount = boxCount - count;

    //tri par rapport a la width (on enleve les trop grandes valeurs)
    double medianeW = calculate_width_mediane(resBox,*newCount);
    double boundMaxW = medianeW * 2.7;

    int widthCount = 0;
    for(int i = 0; i < *newCount; i++)
    {
	if(resBox[i].w > boundMaxW)
	{
		resBox[i].h = 0;
		resBox[i].w = 0;
		widthCount++;
	}
    }

    Box * finalBox = malloc((*newCount - widthCount) * sizeof(Box));
    Box *sortedRes = sort_box(resBox,*newCount);

    for(int j = widthCount ; j < *newCount; j++)
    {
	if(sortedRes[j].h != 0 && sortedRes[j].w != 0)
	{
		finalBox[j - widthCount] = sortedRes[j];
	}

    }

    *newCount = *newCount - widthCount;

    free(resBox);
    free(sortedRes);

    return finalBox;

}


Box* organised_letter_box(Box* box, Box* letterBox, int BoxCount, int letterBoxCount)
{
    int index = 0;
    Box *res = malloc(letterBoxCount * sizeof(Box));
    for(int i = 0; i < BoxCount;i++)
    {
	for(int j = 0; j < letterBoxCount; j++)
	{
		if(box[i].x == letterBox[j].x && box[i].y == letterBox[j].y
			&& box[i].h == letterBox[j].h && box[i].w == letterBox[j].w)
		{
			res[index++] = letterBox[j];
			printf("y:%i\n",letterBox[j].y);
			break;
		}
	}
    }

    if(index != letterBoxCount) 
    {
        printf("ATTENTION: index=%d != letterBoxCount=%d\n", index, letterBoxCount);
    }

    return res;
}


int comparerPoints(const void *a, const void *b) 
{
    const Box* A = (const Box*)a;
    const Box* B = (const Box*)b;

    const int threshold = 5;

    if (abs(A->y - B->y) <= threshold)
        return A->x - B->x;  // trier par X

    return A->y - B->y;}


void middle_box(Box box,int *x,int *y)
{
    *x = box.x + (box.w / 2);
    *y = box.y + (box.h / 2);
}


float  calculate_distance(int x1, int y1, int x2,int y2)
{
	float distanceX = fabsf((float)x1 - (float)x2);
	float distanceY = fabsf((float)y1 - (float)y2);

	return sqrtf(distanceX * distanceX + distanceY * distanceY);
}

void separate_grid_word(Box* boxes, int boxCount, Box*** gridBoxes,
		int** gridCount, Box*** wordBoxes, int** wordCount,
		int* nbLinesGrid, int* nbLinesWord)
{
	qsort(boxes,boxCount,sizeof(Box),comparerPoints);
	printf("boxCount: %i\n",boxCount);
	Box **lines = malloc(boxCount * sizeof(Box*));
	int *sizeLines = malloc(boxCount * sizeof(int));
	int index = 0;
	int count = 0;
	int maxLineSize = 0;
	int nbGridLine = 0;

	while(count < boxCount && index < boxCount)
	{
		int size = 1;

		for(int j = index; j< boxCount - 1;j++)
		{
			int x1 = 0;
			int x2 = 0;
			int y1 = 0;
			int y2 = 0;
			
			middle_box(boxes[j],&x1,&y1);
			middle_box(boxes[j+1],&x2,&y2);

			//printf("x1:%i    x2:%i\n",x1,x2);
                        //printf("y1:%i    y2:%i\n",y1,y2);
			float distance = calculate_distance(x1,y1,x2,y2);

			printf("distance: %f\n",distance);
			if(distance < 70.0) //valeur a determiner
			{
				size++;
			}
			else
			{
				printf("break\n");
				break;
			}
		
		}

		//printf("size: %i\n",size);

		if(size <= 2)
		{
			//index += size;
			index++;
			continue;
		}

		if(size > maxLineSize)
		{
			maxLineSize = size;
		}

		lines[count] = malloc(size * sizeof(Box));
		for(int j = 0; j < size; j++)
		{
			lines[count][j] = boxes[index + j];
			sizeLines[count] = size;
		}

		index += size;
		count++;
	}

	for(int i = 0 ; i < count; i++)
	{
		if(sizeLines[i] >= maxLineSize)
		{
			nbGridLine++;
		}
	}

	*nbLinesGrid = nbGridLine;
	*nbLinesWord = count - nbGridLine;

	*gridBoxes = malloc(nbGridLine * sizeof(Box*));
	*wordBoxes = malloc((*nbLinesWord) * sizeof(Box*));

	*gridCount = malloc(nbGridLine * sizeof(int));
	*wordCount = malloc((*nbLinesWord) * sizeof(int));

	int indexGrid = 0;
	int indexWord = 0;

	for(int i = 0; i < count; i++)
	{
		if(sizeLines[i] >= maxLineSize)
		{
			//gridBoxes[0][indexGrid] = malloc(sizeLines[i] *sizeof(Box));
			(*gridBoxes)[indexGrid] = lines[i];
			//gridCount[indexGrid] = malloc(sizeof(int));
			(*gridCount)[indexGrid] = sizeLines[i];
			indexGrid++;
		}
		else
		{
			//wordBoxes[0][indexWord] = malloc(sizeLines[i] *sizeof(Box));
			(*wordBoxes)[indexWord] = lines[i];
			//wordCount[indexWord] = malloc(sizeof(int));
			(*wordCount)[indexWord] = sizeLines[i];
			indexWord++;
		}
	}

	free(sizeLines);

	printf("finish\n");
}



/*void fillCircleAlpha(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    int x = radius;
    int y = 0;
    int err = 1 - x;

    while (x >= y)
    {
        // 8 points du cercle
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);

        y++;
        if (err < 0)
            err += 2*y + 1;
        else {
            x--;
            err += 2*(y - x + 1);
        }
    }
}*/


void fillCircleAlpha(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    for (int y = -radius; y <= radius; y++) {
        int dx = (int)sqrt(radius * radius - y * y);
        SDL_RenderDrawLine(renderer, cx - dx, cy + y, cx + dx, cy + y);
    }
}






void draw_word(SDL_Renderer *renderer, Box ***boxes,
                            int x1, int y1, int x2, int y2) 
{

    int radius = (*boxes)[y1][x1].w;	
    // Définir la couleur ici (rouge semi-transparent)
    SDL_Color color = {255, 0, 0, 10};

    // Calculer le centre des boxes
    int cx1 = (*boxes)[y1][x1].x + (*boxes)[y1][x1].w / 2;
    int cy1 = (*boxes)[y1][x1].y + (*boxes)[y1][x1].h / 2;
    int cx2 = (*boxes)[y2][x2].x + (*boxes)[y2][x2].w / 2;
    int cy2 = (*boxes)[y2][x2].y + (*boxes)[y2][x2].h / 2;

    // Distance et interpolation
    float dx = cx2 - cx1;
    float dy = cy2 - cy1;
    float dist = sqrt(dx*dx + dy*dy);
    int steps = (int)(dist / (radius * 0.1)); // espacement pour tube continu

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / steps;
        int x = (int)(cx1 + t * dx);
        int y = (int)(cy1 + t * dy);
        fillCircleAlpha(renderer, x, y, radius, color);
    }


    SDL_RenderPresent(renderer);
}



/*float compute_mediane(float* arr, int n)
{
	float* temp = malloc(sizeof(float) * n);
	memcpy(temp, arr, sizeof(float) * n);

	qsort(temp,n,sizeof(float),middle_cmp);

	float med;
	if (n % 2 == 1)
	{
		med = temp[n / 2];
	}
	else
	{
		med = 0.5f * (temp[n/2 - 1] + temp[n/2]);
	}

	free(temp);
	return med;
}

void separate_grid_word(Box* boxes, int boxCount, Box** gridBoxes, int* gridCount, Box** wordBoxes, int* wordCount)
{
	float* centers = malloc(sizeof(float) * boxCount);
	int* labels = malloc(sizeof(int) * boxCount);
	memset(labels, 0, sizeof(int) * boxCount);

	for (int i = 0; i < boxCount; i++)
	{
		centers[i] = boxes[i].y + boxes[i].h * 0.5f;
	}

	float cluster1 = centers[0];
	float cluster2 = centers[boxCount-1];

	int changed = 1;

	while (changed)
	{
		changed = 0;

		for (int i = 0; i < boxCount; i++)
		{
			//caladul de la distance
			float d1 = fabsf(centers[i] - cluster1);
			float d2 = fabsf(centers[i] - cluster2);

			int label = (d1 < d2 ? 0 : 1);

			if (labels[i] != label)
			{
				labels[i] = label;
				changed = 1;
			}
		}


		float sum1 = 0;
		float sum2 = 0;

		int count1 = 0;
		int count2 = 0;

		for (int i = 0; i < boxCount; i++)
		{
			if (labels[i] == 0)
			{
				sum1 += centers[i];
				count1++;
			}
			else
			{
				sum2 += centers[i];
				count2++;
			}
		}

		if (count1 > 0)
		{
			cluster1 = sum1 / count1;
		}

		if (count2 > 0)
		{
			cluster2 = sum2 / count2;
		}
	}

	// Identifier quel cluster est la grille (= le plus bas)
	int gridLabel = (cluster1 > cluster2 ? 0 : 1);

	int tmpGrid = 0;
	int tmpWord = 0;
	for (int i = 0; i < boxCount; i++)
	{
		if(labels[i] == gridLabel)
		{
			tmpGrid++;
		}
		else
		{
			tmpWord++;
		}
	}

	//Calcule la mediane des distances 
	float* tmpGridCenters = malloc(sizeof(float) * tmpGrid);
	float* tmpWordCenters = malloc(sizeof(float) * tmpWord);

	int gi = 0;
	int wi = 0;
	for (int i = 0; i < boxCount; i++)
	{
		if (labels[i] == gridLabel)
		{
			tmpGridCenters[gi++] = centers[i];
		}
		else
		{
			tmpWordCenters[wi++] = centers[i];
		}
	}

	float gridMedian = compute_mediane(tmpGridCenters, tmpGrid);
	float wordMedian = compute_mediane(tmpWordCenters, tmpWord);

	free(tmpGridCenters);
	free(tmpWordCenters);

	//on enleve les distances trop grandes
	float threshold = 150.0f;

	*gridCount = 0;
	*wordCount = 0;
	
	for (int i = 0; i < boxCount; i++)
	{
		if (labels[i] == gridLabel)
		{
			if (fabsf(centers[i] - gridMedian) < threshold)
			{
				(*gridCount)++;
			}
		}
		else
		{
			if (fabsf(centers[i] - wordMedian) < threshold)
			{
				(*wordCount)++;
			}
		}
	}

	*gridBoxes = malloc(sizeof(Box) * (*gridCount));
	*wordBoxes = malloc(sizeof(Box) * (*wordCount));
	printf("test\n");

	int indexG = 0;
	int indexW = 0;
	for (int i = 0; i < boxCount; i++)
	{
		if (labels[i] == gridLabel)
		{
			if (fabsf(centers[i] - gridMedian) < threshold)
			{
				(*gridBoxes)[indexG++] = boxes[i];
			}
		}
		else
		{
			if (fabsf(centers[i] - wordMedian) < threshold)
			{
				(*wordBoxes)[indexW++] = boxes[i];
			}
		}
	}

	free(centers);
	free(labels);

}*/
