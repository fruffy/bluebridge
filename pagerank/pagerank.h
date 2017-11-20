#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct matrix {
	double** elements;
	size_t size;
} matrix;

typedef struct vector {
	double* elements;
	size_t size;
} vector;
