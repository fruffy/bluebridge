#include "structures.h"

// Dynamic array functions
void initArray(IntArr *a, size_t initial_size) {
	a->array = (int*) malloc(initial_size*sizeof(int));
	a->used = 0;
	a->size = initial_size;
}

void insertArray(IntArr *a, int element) {
	if (a->used == a->size) {
		a->size *=2;
		a->array = (int*) realloc(a->array, a->size*sizeof(int));
	}

	a->array[a->used++] = element;
}

void freeArray(IntArr *a) {
	free(a->array);
	a->array = NULL;
	a->used = a->size = 0;
}

// Util functions
void parse_file(char* filename) {
	// TODO: test function
	// Read file line by line

	DEBUG_PRINT(("First read (get max and count)\n"));
	FILE* edges_file = fopen(filename, "r");
	char line[256];
	int v1, v2;
	int max = 0, count = 0;

	while (fgets(line, sizeof(line), edges_file)) {
		sscanf(line, "%d\t%d\n", &v1, &v2);
		if (v1 > max) {
			max = v1;
		}

		if (v2 > max) {
			max = v2;
		}
		count++;
	}

	DEBUG_PRINT(("Rewinding file\n"));
	rewind(edges_file);

	DEBUG_PRINT(("Num vertices: %d, Num edges: %d\n", max+1, count));
	num_vertices = (max+1);

	DEBUG_PRINT(("Mallocing vs array.\n"));
	IntArr* vs = (IntArr*) malloc(num_vertices*sizeof(IntArr));
	
	for (int i = 0; i < num_vertices; i++) {
		initArray(&(vs[i]), 100);
	}

	DEBUG_PRINT(("Mallocing vertices array.\n"));
	vertices = (vertex*) malloc(num_vertices*sizeof(vertex));
	DEBUG_PRINT(("Mallocing edgenorm array.\n"));
	edgenorm = (double*) malloc(num_vertices*sizeof(double));
	DEBUG_PRINT(("Mallocing rank array.\n"));
	rank = (double*) malloc(num_vertices*sizeof(double));
	DEBUG_PRINT(("Mallocing edges array.\n"));
	edges = (double*) malloc(count*sizeof(double));

	DEBUG_PRINT(("Second pass of file.\n"));

	while (fgets(line, sizeof(line), edges_file)) {
		sscanf(line, "%d\t%d\n", &v1, &v2);
		vertices[v1].num_edges++;
		insertArray(&vs[v1], v2);
	}

	DEBUG_PRINT(("closing file.\n"));
	fclose(edges_file);

	DEBUG_PRINT(("Setting rest of variables.\n"));
	int k = 0; 
	for (int i = 0; i < max+1; i++) {
		rank[i] = 1;
		edgenorm[i] = vertices[i].num_edges + 1;
		vertices[i].edge_offset = k;

		for (int j = 0; j < vs[i].size; j++) {
			edges[k] = vs[i].array[j];
			k++;
		}
	}

	DEBUG_PRINT(("Freeing dynamic array.\n"));
	for (int i = 0; i < num_vertices; i++) {
		freeArray(&vs[i]);
	}

	free(vs);
}





















// Vector functions
void init_vector(vector* v, int size) {
	v->size = (size_t) size;
	v->elements = (double*)malloc(sizeof(double*)*size);
}

void print_vector(vector* v, char* message, int force) {
	if (DEBUG || (force && !DEBUG)){
		printf("%s\n", message);
		for (int i = 0; i < v->size; i++) {
			printf("\t%f\n", v->elements[i]);
		}
	}
}

vector scalar_vector_multiply(double s, vector* v) {
	vector res;
	init_vector(&res, v->size);

	for (int i = 0; i < v->size; i++) {
		res.elements[i] = v->elements[i]*s;
	}

	return res;
}

vector vector_add(vector* v1, vector* v2) {
	vector res;
	init_vector(&res, v1->size);

	for (int i = 0; i < v1->size; i++) {
		res.elements[i] = v1->elements[i] + v2->elements[i];
	}

	return res;
}

vector vector_subtract(vector* v1, vector* v2) {
	vector res;
	init_vector(&res, v1->size);

	for (int i = 0; i < v1->size; i++) {
		res.elements[i] = v1->elements[i] - v2->elements[i];
	}

	return res;
}

double euclidean_norm_vector(vector* v) {
	double res = 0; 

	for (int i = 0; i < v->size; i++) {
		res += v->elements[i]*v->elements[i];
	}

	return sqrt(res);
}

// Matrix functions
void print_matrix(matrix* m, char* message) {
	if (DEBUG){
		printf("%s\n", message);
		for (int i = 0; i < m->size; i++) {
			printf("\t");
			for (int j = 0; j < m->size; j++) {
				printf("%f ", m->elements[i][j]);
			}
			printf("\n");
		}
	}
}

void init_matrix(matrix* m, int size) {
	m->size = (size_t) size;
	m->elements = (double**)malloc(sizeof(double*)*size);

	for (int i = 0; i < size; i++) {
		m->elements[i] = malloc(sizeof(double)*size);
	}
}

vector matrix_vector_multiply(matrix* m, vector* v) {
	vector res;
	init_vector(&res, v->size);

	for(int i = 0; i < v->size; i++) { // rows
		double sum = 0; 
		for (int j = 0; j < v->size; j ++) { // columns
			sum += m->elements[i][j]*v->elements[j];
		}

		res.elements[i] = sum;
	}

	return res;
}