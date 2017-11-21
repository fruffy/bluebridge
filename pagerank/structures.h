#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEBUG 0

typedef struct {
	int *array;
	size_t used;
	size_t size;
} IntArr;

void initArray(IntArr *a, size_t initial_size);

void insertArray(IntArr *a, int element);

void freeArray(IntArr *a);

typedef struct vertex {
	double incoming_rank;	// Incoming rank for this vertex
	int num_edges;		// Number of edges
	int edge_offset;	// Offset into the edges array
} vertex;

double* edges; 		// An array of all outgoing edges.
double* edgenorm; 	// number of outgoing edges for vertex j
vertex* vertices; 	// An array of all vertices in the graph
double* rank;		// Page rank value for vertex j
IntArr	ids;		// List of array ids (dynamically growing)
int 	num_vertices;	// Number of vertices in graph

// Parse file - populate structures using IntArr
void parse_file(char* filename);

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