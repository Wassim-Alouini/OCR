#include "detectgrid.h"
#include <time.h>


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

int distance_cmp(const void *first, const void *second)
{
    float a = *(const float*)first;
    float b = *(const float*)second;
    if (a < b) return -1;
    if (a > b) return  1;
    return 0;
}



//_____________________________________________________________TESTING FUNCTIONS


typedef struct {
    float v;
} F1;

static int cmp_f1(const void *a, const void *b) {
    float x = ((const F1*)a)->v;
    float y = ((const F1*)b)->v;
    return (x < y) ? -1 : (x > y);
}

static float best_gap_1d(Box *boxes, int n, int axis /*0=x,1=y*/, float *outSplit)
{
    if (!boxes || n < 2) { if(outSplit) *outSplit = 0.f; return 0.f; }

    F1 *arr = malloc(sizeof(F1) * n);
    for (int i = 0; i < n; i++) {
        float c = (axis == 0)
            ? (float)boxes[i].x + (float)boxes[i].w * 0.5f
            : (float)boxes[i].y + (float)boxes[i].h * 0.5f;
        arr[i].v = c;
    }

    qsort(arr, n, sizeof(F1), cmp_f1);

    float bestGap = 0.f;
    int bestK = -1;
    for (int i = 0; i < n - 1; i++) {
        float g = arr[i+1].v - arr[i].v;
        if (g > bestGap) { bestGap = g; bestK = i; }
    }

    if (outSplit) {
        if (bestK >= 0) *outSplit = 0.5f * (arr[bestK].v + arr[bestK+1].v);
        else *outSplit = 0.f;
    }

    free(arr);
    return bestGap;
}

static int split_is_reasonable(Box *boxes, int n, int axis, float splitVal)
{
    // require both groups to be non-trivial (avoid a single outlier far away)
    int left = 0, right = 0;
    for (int i = 0; i < n; i++) {
        float c = (axis == 0)
            ? (float)boxes[i].x + (float)boxes[i].w * 0.5f
            : (float)boxes[i].y + (float)boxes[i].h * 0.5f;
        if (c <= splitVal) left++;
        else right++;
    }

    // at least 15% of points on each side (tune if needed)
    int minSide = (int)ceilf(n * 0.15f);
    if (left < minSide || right < minSide) return 0;
    return 1;
}

int should_use_separate_v2(Box *boxes, int boxCount, float medianHeight)
{
    // V2 is good when there is a strong X split (side-by-side layout).
    // V1 is better when Y split is stronger or X split is not clean.

    float splitX = 0.f, splitY = 0.f;
    float gapX = best_gap_1d(boxes, boxCount, 0, &splitX);
    float gapY = best_gap_1d(boxes, boxCount, 1, &splitY);

    // minimum gap to consider a real separation (tune)
    float minGap = medianHeight * 2.0f;

    int xOK = (gapX >= minGap) && split_is_reasonable(boxes, boxCount, 0, splitX);
    int yOK = (gapY >= minGap) && split_is_reasonable(boxes, boxCount, 1, splitY);

    // If X split exists and is stronger than Y => side-by-side => use V2
    if (xOK && (!yOK || gapX > gapY * 1.1f))
        return 1;

    return 0; // otherwise prefer V1
}

typedef struct {
    Box *items;
    int  size;
    int  cap;
    float yref;
} Line;

static void line_push(Line *L, Box b) {
    if (L->size == L->cap) {
        L->cap = (L->cap == 0) ? 16 : L->cap * 2;
        L->items = realloc(L->items, L->cap * sizeof(Box));
    }
    L->items[L->size++] = b;
}

static int cmp_ycenter(const void *a, const void *b) {
    const Box *A = a, *B = b;
    int ya = A->y + A->h/2;
    int yb = B->y + B->h/2;
    return ya - yb;
}

static int cmp_x(const void *a, const void *b) {
    const Box *A = a, *B = b;
    return A->x - B->x;
}

typedef struct {
    float meanGap;
    float stdGap;
    int   bigGapCount;
    float width;
} LineMetrics;

static LineMetrics compute_line_metrics(Box *items, int n)
{
    LineMetrics m = {0};
    if (!items || n <= 1) return m;

    // line width
    int xmin = items[0].x;
    int xmax = items[0].x + items[0].w;
    for (int i = 1; i < n; i++) {
        if (items[i].x < xmin) xmin = items[i].x;
        int r = items[i].x + items[i].w;
        if (r > xmax) xmax = r;
    }
    m.width = (float)(xmax - xmin);

    // gaps between consecutive boxes (sorted by x is assumed)
    int gapCount = 0;
    float sum = 0.f;

    for (int i = 0; i < n - 1; i++) {
        int right = items[i].x + items[i].w;
        int gap = items[i+1].x - right;
        if (gap < 0) gap = 0; // overlap -> treat as 0
        sum += (float)gap;
        gapCount++;
    }

    if (gapCount == 0) return m;

    m.meanGap = sum / (float)gapCount;

    // std dev
    float var = 0.f;
    for (int i = 0; i < n - 1; i++) {
        int right = items[i].x + items[i].w;
        int gap = items[i+1].x - right;
        if (gap < 0) gap = 0;
        float d = (float)gap - m.meanGap;
        var += d * d;
    }
    var /= (float)gapCount;
    m.stdGap = sqrtf(var);

    // "big gaps" (word breaks) count
    float bigT = (m.meanGap * 2.2f) + 2.0f;   // tune 2.0..3.0
    int big = 0;
    for (int i = 0; i < n - 1; i++) {
        int right = items[i].x + items[i].w;
        int gap = items[i+1].x - right;
        if (gap < 0) gap = 0;
        if ((float)gap > bigT) big++;
    }
    m.bigGapCount = big;

    return m;
}

static float box_xcenter(const Box *b) {
    return (float)(b->x + b->w * 0.5f);
}

static float box_ycenter(const Box *b) {
    return (float)(b->y + b->h * 0.5f);
}

// Insert xc into column centers (merge if within tol)
static void columns_add(float **cols, int *ncols, int *cap, float xc, float tol)
{
    // try merge
    for (int i = 0; i < *ncols; i++) {
        if (fabsf(xc - (*cols)[i]) <= tol) {
            // running average (simple)
            (*cols)[i] = (*cols)[i] * 0.8f + xc * 0.2f;
            return;
        }
    }
    // new column
    if (*ncols == *cap) {
        *cap = (*cap == 0) ? 16 : (*cap * 2);
        *cols = realloc(*cols, (*cap) * sizeof(float));
    }
    (*cols)[(*ncols)++] = xc;
}

// How many boxes in the line match known columns
static float line_column_match_ratio(Box *items, int n, const float *cols, int ncols, float tol)
{
    if (!items || n <= 0 || !cols || ncols <= 0) return 0.f;

    int matched = 0;
    for (int i = 0; i < n; i++) {
        float xc = box_xcenter(&items[i]);
        for (int c = 0; c < ncols; c++) {
            if (fabsf(xc - cols[c]) <= tol) {
                matched++;
                break;
            }
        }
    }
    return (float)matched / (float)n;
}

static void build_lines_from_boxes(Box *arr, int n, float medianHeight,
                                   Box ***outBoxes, int **outCount, int *outLines)
{
    *outBoxes = NULL;
    *outCount = NULL;
    *outLines = 0;
    if (!arr || n <= 0) return;

    qsort(arr, n, sizeof(Box), cmp_ycenter);

    float yThreshold = medianHeight * 0.6f;

    Line *lines = NULL;
    int lineCount = 0, lineCap = 0;

    for (int i = 0; i < n; i++) {
        int yc = arr[i].y + arr[i].h / 2;

        if (lineCount == 0 || fabsf((float)yc - lines[lineCount - 1].yref) > yThreshold) {
            if (lineCount == lineCap) {
                lineCap = (lineCap == 0) ? 16 : lineCap * 2;
                lines = realloc(lines, lineCap * sizeof(Line));
            }
            lines[lineCount] = (Line){0};
            lines[lineCount].yref = (float)yc;
            lineCount++;
        }

        line_push(&lines[lineCount - 1], arr[i]);
        lines[lineCount - 1].yref = lines[lineCount - 1].yref * 0.8f + (float)yc * 0.2f;
    }

    for (int i = 0; i < lineCount; i++)
        qsort(lines[i].items, lines[i].size, sizeof(Box), cmp_x);

    *outBoxes = malloc(lineCount * sizeof(Box*));
    *outCount = malloc(lineCount * sizeof(int));
    *outLines = lineCount;

    for (int i = 0; i < lineCount; i++) {
        (*outBoxes)[i] = lines[i].items; // ownership transferred to caller
        (*outCount)[i] = lines[i].size;
    }

    free(lines); // do NOT free lines[i].items
}

static int mode_of_ints(const int *a, int n, int *outFreq)
{
    int bestVal = 0, bestFreq = 0;
    for (int i = 0; i < n; i++) {
        int v = a[i];
        int f = 0;
        for (int j = 0; j < n; j++)
            if (a[j] == v) f++;
        if (f > bestFreq) { bestFreq = f; bestVal = v; }
    }
    if (outFreq) *outFreq = bestFreq;
    return bestVal;
}

static float grid_likeness(Box **lines, int *counts, int nLines)
{
    if (!lines || !counts || nLines <= 0) return 0.f;

    int modeFreq = 0;
    (void)mode_of_ints(counts, nLines, &modeFreq);

    int total = 0;
    for (int i = 0; i < nLines; i++) total += counts[i];

    // Score: lots of boxes + many rows share the same length
    // (tweak weights if needed)
    return 1.0f * (float)modeFreq + 0.01f * (float)total;
}

void separate_grid_word_v2(Box *boxes, int boxCount,
    Box ***gridBoxes, int **gridCount,
    Box ***wordBoxes, int **wordCount,
    int *nbLinesGrid, int *nbLinesWord,
    float medianHeight)
{
    if (!boxes || boxCount <= 0) {
        *gridBoxes = NULL; *gridCount = NULL;
        *wordBoxes = NULL; *wordCount = NULL;
        *nbLinesGrid = 0; *nbLinesWord = 0;
        return;
    }

    // ---------- 1) split into LEFT (grid) and RIGHT (words) by biggest X gap ----------
    float *xc = malloc(sizeof(float) * boxCount);
    int   *idx = malloc(sizeof(int) * boxCount);

    for (int i = 0; i < boxCount; i++) {
        xc[i] = (float)boxes[i].x + (float)boxes[i].w * 0.5f;
        idx[i] = i;
    }

    // sort idx by xc (simple O(n^2) sort; n is not huge)
    for (int i = 0; i < boxCount - 1; i++) {
        for (int j = i + 1; j < boxCount; j++) {
            if (xc[idx[j]] < xc[idx[i]]) {
                int tmp = idx[i];
                idx[i] = idx[j];
                idx[j] = tmp;
            }
        }
    }

    // find largest gap
    float bestGap = 0.f;
    int bestK = -1;
    for (int k = 0; k < boxCount - 1; k++) {
        float g = xc[idx[k+1]] - xc[idx[k]];
        if (g > bestGap) { bestGap = g; bestK = k; }
    }

    float splitMinGap = medianHeight * 2.0f; // tune 1.5..3.0
    int hasSplit = (bestK >= 0 && bestGap >= splitMinGap);
    float splitX = 0.f;

    if (hasSplit) {
        splitX = 0.5f * (xc[idx[bestK]] + xc[idx[bestK+1]]);
    }

    Box *left  = malloc(sizeof(Box) * boxCount);
    Box *right = malloc(sizeof(Box) * boxCount);
    int nL = 0, nR = 0;

    for (int i = 0; i < boxCount; i++) {
        float xci = (float)boxes[i].x + (float)boxes[i].w * 0.5f;
        if (hasSplit && xci > splitX) right[nR++] = boxes[i];
        else left[nL++] = boxes[i];
    }

    free(xc);
    free(idx);

    // ---------- 2) build lines separately on each side ----------
	Box **leftLines = NULL, **rightLines = NULL;
	int *leftCnt = NULL, *rightCnt = NULL;
	int leftN = 0, rightN = 0;

	build_lines_from_boxes(left,  nL, medianHeight, &leftLines,  &leftCnt,  &leftN);
	build_lines_from_boxes(right, nR, medianHeight, &rightLines, &rightCnt, &rightN);

	// decide which side is the grid
	float sL = grid_likeness(leftLines, leftCnt, leftN);
	float sR = grid_likeness(rightLines, rightCnt, rightN);

	// if right is more grid-like, swap
	if (sR > sL) {
		*gridBoxes = rightLines;  *gridCount = rightCnt;  *nbLinesGrid = rightN;
		*wordBoxes = leftLines;   *wordCount = leftCnt;   *nbLinesWord = leftN;
	} else {
		*gridBoxes = leftLines;   *gridCount = leftCnt;   *nbLinesGrid = leftN;
		*wordBoxes = rightLines;  *wordCount = rightCnt;  *nbLinesWord = rightN;
	}


    free(left);
    free(right);
}


//_______________________________________________________END TESTING

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

    const int threshold = 7;

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


float calculate_distance_mediane(float* array, int count)
{
    float mediane = 0;
    int middle = count / 2;

    if(count % 2 == 0)
    {
		double distance1 = array[middle];
		double distance2 = array[middle - 1];
		mediane = (distance1 + distance2) / 2;
		
		return mediane;
    }

    mediane = array[middle];
    return mediane;
}


float median_distance(Box* boxes, int boxCount)
{
	float *array = malloc((boxCount - 1) * sizeof(float));
	for(int i = 0; i < boxCount - 1; i++)
	{
		int x1 = 0;
		int x2 = 0;
		int y1 = 0;
		int y2 = 0;
	
		middle_box(boxes[i],&x1,&y1);
		middle_box(boxes[i+1],&x2,&y2);

		float distance = calculate_distance(x1,y1,x2,y2);

		array[i] = distance;
	}

	qsort(array,boxCount - 1,sizeof(float),distance_cmp);

	float mediane = calculate_distance_mediane(array,boxCount - 1);
	free(array);
	return mediane;
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
	float threshold = median_distance(boxes,boxCount) + 20.0;
	printf("distance mediane : %f\n",threshold);

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

			printf("distance: %f between [x = %i, y = %i] and [x = %i, y = %i]\n",distance, x1, y1, x2, y2);
			if(distance < threshold) //valeur a determiner 70.0
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

	printf("\n--- draw_word ---\n");
	printf("boxes=%p\n", boxes);
	printf("*boxes=%p\n", *boxes);
	printf("y1=%d  y2=%d  x1=%d  x2=%d\n", y1, y2, x1, x2);

	printf("row y1 = %p\n", (*boxes)[y1]);
	printf("row y2 = %p\n", (*boxes)[y2]);


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

// Helper: blend a single pixel with alpha (surface as SDL_Surface **)
static void putPixelAlpha(SDL_Surface **surface, int x, int y, SDL_Color color)
{
    if (!surface || !*surface) return;

    SDL_Surface *s = *surface;

    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;

    Uint8 sr = color.r;
    Uint8 sg = color.g;
    Uint8 sb = color.b;
    Uint8 sa = color.a; // 0..255

    // Assume 32bpp surface
    Uint32 *pixels = (Uint32 *)s->pixels;
    int pitch = s->pitch / 4; // Uint32 per line
    Uint32 *p = &pixels[y * pitch + x];

    Uint8 dr, dg, db, da;
    SDL_GetRGBA(*p, s->format, &dr, &dg, &db, &da);

    Uint8 invA = 255 - sa;

    Uint8 outR = (Uint8)((sr * sa + dr * invA) / 255);
    Uint8 outG = (Uint8)((sg * sa + dg * invA) / 255);
    Uint8 outB = (Uint8)((sb * sa + db * invA) / 255);
    Uint8 outA = (Uint8)((sa + da * invA / 255)); // optional alpha update

    *p = SDL_MapRGBA(s->format, outR, outG, outB, outA);
}

// Helper: filled circle with alpha on a surface** (no lock inside)
static void fillCircleAlphaSurface(SDL_Surface **surface, int cx, int cy, int radius, SDL_Color color)
{
    if (!surface || !*surface || radius <= 0) return;

    int r2 = radius * radius;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2) {
                putPixelAlpha(surface, cx + dx, cy + dy, color);
            }
        }
    }
}

// Surface** version of draw_word

static float frand01(void) { return (float)rand() / (float)RAND_MAX; }

static void hsv_to_rgb(float h, float s, float v, Uint8 *outR, Uint8 *outG, Uint8 *outB)
{
    // h in [0, 1)
    float r=0, g=0, b=0;
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);

    switch ((int)i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
    }

    *outR = (Uint8)(r * 255.0f);
    *outG = (Uint8)(g * 255.0f);
    *outB = (Uint8)(b * 255.0f);
}

// bright + saturated random color (good visibility)
static SDL_Color random_vivid_color(void)
{
    float h = frand01();                         // random hue
    float s = 0.80f + 0.20f * frand01();         // saturation 0.8..1.0
    float v = 0.85f + 0.15f * frand01();         // value 0.85..1.0

    Uint8 r,g,b;
    hsv_to_rgb(h, s, v, &r, &g, &b);

    SDL_Color c = { r, g, b, 20 };              // fully opaque
    return c;
}
void draw_word_surface(SDL_Surface **surface, Box ***boxes,
                       int x1, int y1, int x2, int y2)
{
    printf("\n--- draw_word_surface ---\n");
    printf("surface**=%p\n", (void*)surface);
    printf("*surface =%p\n", surface ? (void*)(*surface) : NULL);
    printf("boxes=%p\n", (void*)boxes);
    printf("*boxes=%p\n", boxes ? (void*)(*boxes) : NULL);
    printf("y1=%d  y2=%d  x1=%d  x2=%d\n", y1, y2, x1, x2);

    if (!surface || !*surface || !boxes || !(*boxes)) {
        printf("ERROR: invalid args in draw_word_surface\n");
        return;
    }

    SDL_Surface *s = *surface;

    printf("  w=%d h=%d pitch=%d Bpp=%d\n",
           s->w, s->h, s->pitch, s->format->BytesPerPixel);

    if (s->format->BytesPerPixel != 4) {
        printf("ERROR: draw_word_surface expects 32bpp surface, got %d Bpp\n",
               s->format->BytesPerPixel);
        return; // prevents segfault
    }

    printf("row y1 = %p\n", (void*)(*boxes)[y1]);
    printf("row y2 = %p\n", (void*)(*boxes)[y2]);

	int radius = (*boxes)[y1][x1].h;

    SDL_Color color = random_vivid_color();

    int cx1 = (*boxes)[y1][x1].x + (*boxes)[y1][x1].w / 2;
    int cy1 = (*boxes)[y1][x1].y + (*boxes)[y1][x1].h / 2;
    int cx2 = (*boxes)[y2][x2].x + (*boxes)[y2][x2].w / 2;
    int cy2 = (*boxes)[y2][x2].y + (*boxes)[y2][x2].h / 2;

    float dx = (float)(cx2 - cx1);
    float dy = (float)(cy2 - cy1);
    float dist = sqrtf(dx * dx + dy * dy);
    int steps = (int)(dist / (radius * 0.1f));
    if (steps < 1) steps = 1;

    if (SDL_MUSTLOCK(s)) {
        if (SDL_LockSurface(s) != 0) {
            printf("ERROR: SDL_LockSurface failed: %s\n", SDL_GetError());
            return;
        }
    }

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int x = (int)(cx1 + t * dx);
        int y = (int)(cy1 + t * dy);
        fillCircleAlphaSurface(surface, x, y, radius, color);
    }

    if (SDL_MUSTLOCK(s)) {
        SDL_UnlockSurface(s);
    }
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
