#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEBUG 0

// Vector functions and definition
typedef struct vector {
	double* elements;
	size_t size;
} vector;

void init_vector(vector* v, int size);

void print_vector(vector* v, char* message, int force);

vector scalar_vector_multiply(double s, vector* v);

vector vector_add(vector* v1, vector* v2);

vector vector_subtract(vector* v1, vector* v2);

double euclidean_norm_vector(vector* v);

// Matrix functions and definition
typedef struct matrix {
	double** elements;
	size_t size;
} matrix;

void init_matrix(matrix* m, int size);

void print_matrix(matrix* m, char* message);

vector matrix_vector_multiply(matrix* m, vector* v);