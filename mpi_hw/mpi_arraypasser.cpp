/*
 ============================================================================
 Name        : mpi_run.c
 Author      : dominic
 Version     :
 Copyright   : Your copyright notice
 Description : Compute Pi in MPI C++
 ============================================================================
 */
#include <math.h> 
#include "mpi.h" 
#include <iostream>
using namespace std;

void Allgather_ring(float, int, float[], MPI_Comm);

void Allgather_ring(float x[], int blocksize, float y[], MPI_Comm ring_comm){
	int i, p, my_rank;
	int successor, predecessor;
	int send_offset, recv_offset;
	MPI_Status status;

	MPI_Comm_size(ring_comm, &p);
	MPI_Comm_rank(ring_comm, &my_rank);

	MPI_Request send_request;
	MPI_Request recv_request;

	/* Copy x into correct location in y */
	for (i=0; i<blocksize; i++)
			y[i+my_rank*blocksize] = x[i];
	successor = (my_rank +1)%p;
	predecessor = (my_rank -1 +0)%p;

	send_offset = my_rank*blocksize;
	recv_offset = ((my_rank -1 +p)%p)*blocksize;
	for (int i = 0; i<p-1; i++){
		MPI_Isend(y + send_offset, blocksize, MPI_FLOAT, successor,   0, ring_comm, &send_request);
		MPI_Irecv(y + recv_offset, blocksize, MPI_FLOAT, predecessor, 0, ring_comm, &recv_request);

		send_offset = ((my_rank -i -1 +p)%p)*blocksize;
		recv_offset = ((my_rank -i -2 +p)%p)*blocksize;

		MPI_Wait(&send_request, &status);
		MPI_Wait(&recv_request, &status);
	}//for
}//Allgather_ring




int main(int argc, char *argv[]) {
	int n, rank, size, i;
	double PI25DT = 3.141592653589793238462643;
	double mypi, pi, h, sum, x;

	MPI::Init(argc, argv);
	size = MPI::COMM_WORLD.Get_size();
	rank = MPI::COMM_WORLD.Get_rank();

	n=1000; // number of intervals

	MPI::COMM_WORLD.Bcast(&n, 1, MPI::INT, 0);
	h = 1.0 / (double) n;
	sum = 0.0;
	for (i = rank + 1; i <= n; i += size) {
		x = h * ((double) i - 0.5);
		sum += (4.0 / (1.0 + x * x));
	}
	mypi = h * sum;

	MPI::COMM_WORLD.Reduce(&mypi, &pi, 1, MPI::DOUBLE, MPI::SUM, 0);
	if (rank == 0)
		cout << "pi is approximately " << pi << ", Error is "
				<< fabs(pi - PI25DT) << endl;

	float *full_arr;
	float *block;
	int blocksize = 1024;

	int arraysize = blocksize*rank;
	full_arr = new float[arraysize];
	block = new float[blocksize];
	Allgather_ring(full_arr, blocksize, block, MPI_COMM_WORLD);
	printf("Im done!!");

	MPI::Finalize();
	return 0;
}

