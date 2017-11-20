#import "structures.h"

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