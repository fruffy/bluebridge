#include "structures.h"

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
	int r = 0;
	// For each entry, set entry to 1, go column wise to get sum. 
	while (!feof(adj_mat)) { // iterate through rows
		int c = 0;
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

	fclose(adj_mat);

	// For each column divide each element by the sum
	for (int j = 0; j < size; j++) { // columns
		for (int i = 0; i < size; i++) { // rows
			if (res->elements[i][j] != 0) {
				res->elements[i][j] = res->elements[i][j]/col_sums[j];
			}
		}
	}

	// DEBUG
	if (DEBUG) {
		for (int i = 0; i < size; i++) {
			for(int j = 0; j < size; j++) {
				printf("%f ", res->elements[i][j]);
			}
			printf("\n");
		}
	}
}

void initialize_data(vector* damping_vector, vector* r_vector, matrix*
	link_values, double d, int N, char* mat_file) {
	// Create damping factor vector ((d-1)/N) for all values
	init_vector(damping_vector, N);

	for (int i = 0; i < N; i++) {
		damping_vector->elements[i] = (1 - d)/((double) N);
	}

	print_vector(damping_vector, "Damping vector", 0);

	// Create link values
	init_matrix(link_values, N);
	generate_link_data(mat_file, link_values, N);

	print_matrix(link_values, "Link values");

	// Create R vector (initial value of 1/N)
	init_vector(r_vector, N);

	for (int i = 0; i < N; i++) {
		r_vector->elements[i] = ((double) 1)/((double) N);
	}

	print_vector(r_vector, "Initial r vector", 0);
}

void usage() {
	printf("./pagerank <damp factor> <error> <num nodes> <adj mat filename>\n");
}

void pagerank(int rounds, int d) {
	double outrank = 0;
	double alpha = ((double) (1 - d))/((double) num_vertices);

	for (int i = 0; i < rounds; i++) {
		for (int j = 0; j < num_vertices; j++) {
			outrank = rank[j]/edgenorm[j];
			for (int k = 0; k < vertices[j].num_edges; k++) {
				// TODO: check values
				int edge_index = vertices[j].edge_offset + k;
				int edge_to = edges[edge_index];
				vertex to_vertex = vertices[edge_to];
				to_vertex.incoming_rank += outrank;
			}
		}

		for (int j = 0; j < num_vertices; j++) {
			rank[j] = alpha + (d*vertices[j].incoming_rank);
		}
	}
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
	vector damping_vector, r_vector;
	matrix link_values;

	initialize_data(&damping_vector, &r_vector, &link_values, d, N, filename);

	// Execute pagerank
	vector old_r;
	vector difference;

	int iteration = 0; 
	do {
		old_r = r_vector;
		vector update_r_vector = matrix_vector_multiply(&link_values, &r_vector);
		print_vector(&update_r_vector, "Updated r vector", 0);
		vector damped_updated_r_vector = scalar_vector_multiply(d, &update_r_vector);
		print_vector(&damped_updated_r_vector, "Damped updated r vector", 0);
		r_vector = vector_add(&damping_vector, &damped_updated_r_vector);
		print_vector(&r_vector, "New r vector", 0);
		difference = vector_subtract(&r_vector, &old_r);
		print_vector(&difference, "Difference vector", 0);
		if (DEBUG)
			printf("Iteration: %d, Norm: %f\n", iteration, euclidean_norm_vector(&difference));
		iteration++;
	} while(euclidean_norm_vector(&difference) >= err);

	print_vector(&r_vector, "PageRank Results:", 1);
	printf("Total Iterations: %d\n", iteration);
	return 0;
}