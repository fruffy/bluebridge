#import "pagerank.h"

void init_vector(vector* v, int size) {
	v->size = (size_t) size;
	v->elements = (double*)malloc(sizeof(double*)*size);
}

void print_vector(vector* v, char* message) {
	printf("%s\n", message);
	for (int i = 0; i < v->size; i++) {
		printf("\t%f\n", v->elements[i]);
	}
}

void print_matrix(matrix* m, char* message) {
	printf("%s\n", message);
	for (int i = 0; i < m->size; i++) {
		printf("\t");
		for (int j = 0; j < m->size; j++) {
			printf("%f ", m->elements[i][j]);
		}
		printf("\n");
	}
}

void init_matrix(matrix* m, int size) {
	m->size = (size_t) size;
	m->elements = (double**)malloc(sizeof(double*)*size);

	for (int i = 0; i < size; i++) {
		m->elements[i] = malloc(sizeof(double)*size);
	}
}

void generate_link_data(char* filename, matrix* res, int size) {
	// printf("Generating link data\n");
	// array to hold column sums
	double col_sums[size];

	for (int index = 0; index < size; index++) {
		col_sums[index] = 0;
	}

	// Open adjacency matrix file
	FILE *adj_mat = fopen(filename, "r");

	// printf("Reading file...\n");
	int c, r = 0;
	// For each entry, set entry to 1, go column wise to get sum. 
	while (!feof(adj_mat)) { // iterate through rows
		c = 0;
		char val;
		do { // iterate through columns
			val = (char)fgetc(adj_mat);
			// printf("Got char %c\n", val);

			if (feof(adj_mat)) {
				// printf("end of file\n");
				break;
			}

			if (val != ' ' && val !='\n') {
				int connected = val - '0';
				// printf("\tConnected: %d\n", connected);
				res->elements[c][r] = connected; // Flip r,c b/c transpose
				col_sums[r] += connected;
				// printf("col_sums[%d] = %f\n", c, col_sums[c]);
				c++;
			}
		} while (val != '\n');
		r++;
	}

	// For each column divide each element by the sum
	for (int j = 0; j < size; j++) { // columns
		for (int i = 0; i < size; i++) { // rows
			if (res->elements[i][j] != 0) {
				res->elements[i][j] = res->elements[i][j]/col_sums[j];
			}
		}
	}

	// DEBUG
	for (int i = 0; i < size; i++) {
		for(int j = 0; j < size; j++) {
			printf("%f ", res->elements[i][j]);
		}
		printf("\n");
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

void usage() {
	printf("./pagerank <damp factor> <error> <num nodes> <adj mat filename>\n");
}

int main(int argc, char** argv) {
	// Read in arguments (d, err, adj mat)
	if (argc != 5) {
		// print usage
		usage();
		return -1;
	}

	char* ptr;

	double d = strtod(argv[1], &ptr); 	// damping factor variable
	double err = strtod(argv[2], &ptr); 	// error to converge to
	int N = strtol(argv[3], &ptr, 10); 	// number of nodes

	char* filename = argv[4];	// file containing adj mat

	// Setup info (damping factor vector, link values, R vector)
	
	// Create damping factor vector ((d-1)/N) for all values
	vector damping_vector;
	init_vector(&damping_vector, N);

	for (int i = 0; i < N; i++) {
		damping_vector.elements[i] = (1 - d)/((double) N);
	}

	print_vector(&damping_vector, "Damping vector");

	vector d_vector;
	init_vector(&d_vector, N);

	for (int i = 0; i < N; i++) {
		d_vector.elements[i] = 0;///((double) N);
	}

	d_vector.elements[0] = d;

	// Create link values
	matrix link_values;
	init_matrix(&link_values, N);
	generate_link_data(filename, &link_values, N);

	print_matrix(&link_values, "Link values");

	// Create R vector (initial value of 1/N)
	vector r_vector;
	init_vector(&r_vector, N);

	for (int i = 0; i < N; i++) {
		r_vector.elements[i] = ((double) 1)/((double) N);
	}

	print_vector(&r_vector, "Initial r vector");

	// Execute pagerank
	vector old_r;
	vector difference;

	int iteration = 0; 
	do {
		old_r = r_vector;
		vector update_r_vector = matrix_vector_multiply(&link_values, &r_vector);
		print_vector(&update_r_vector, "Updated r vector");
		vector damped_updated_r_vector = scalar_vector_multiply(d, &update_r_vector);
		print_vector(&damped_updated_r_vector, "Damped updated r vector");
		r_vector = vector_add(&damping_vector, &damped_updated_r_vector);
		print_vector(&r_vector, "New r vector");
		// r_vector = vector_add(&r_vector, &d_vector);
		// print_vector(&r_vector, "r vector after adding d");
		difference = vector_subtract(&r_vector, &old_r);
		print_vector(&difference, "Difference vector");
		printf("Iteration: %d, Norm: %f\n", iteration, euclidean_norm_vector(&difference));
		iteration++;
	} while(euclidean_norm_vector(&difference) >= err);

	print_vector(&r_vector, "Final r vector");
	return 0;
}