#include <stdio.h>
#include <string.h> // For strlen //
#include <mpi.h> // For MPI functions, etc //

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#define PI 3.14159265

using namespace std;

//const int NULL = 0;
const int MAX_STRING = 200;

int main(void) {
	char greeting[MAX_STRING], recv_greeting[MAX_STRING];
	int comm_sz; // Number of processes //
	int my_rank; // My process rank //
	

	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Status status;
	MPI_Request send_request, recv_request;

	int count;
	//Greetings
	if (my_rank != 0) {
		sprintf(greeting, "Greetings from process %d of %d!\n",
		my_rank, comm_sz);
		MPI_Send(greeting, strlen(greeting)+1, MPI_CHAR, 0, 0,
		MPI_COMM_WORLD);
	 } else {
		printf("Greetings from process %d of %d!\n", my_rank, comm_sz);
		for (int q = 1; q < comm_sz; q++) {
		MPI_Recv(greeting, MAX_STRING, MPI_CHAR, q,
			   0, MPI_COMM_WORLD, &status);
			MPI_Get_count(&status, MPI_CHAR, &count);
			printf("%s  [MPI_Status src:%i count:%i] \n", greeting, status.MPI_SOURCE, count);
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	printf("post-Barrier %i\n", my_rank);



	//MPI_Isend _Irecv
	{
	sprintf(greeting, "unitialized");
	if (my_rank == 0){
		sprintf(greeting, "Isend Greetings from process %d of %d!\n", my_rank, comm_sz);
		for (int q = 1; q < comm_sz; q++) {
		MPI_Isend(greeting, strlen(greeting)+1, MPI_CHAR, q, 0, MPI_COMM_WORLD, &send_request);
		MPI_Wait(&send_request, &status);
		}//for
	}else{
	
		MPI_Irecv(recv_greeting, MAX_STRING, MPI_CHAR, 0,  0, MPI_COMM_WORLD, &recv_request);
		MPI_Wait(&recv_request, &status);
		MPI_Get_count(&status, MPI_CHAR, &count);
		printf("%s  [post-send MPI_Status src:%i count:%i] \n", recv_greeting, status.MPI_SOURCE, count);
	}
	
	MPI_Get_count(&status, MPI_CHAR, &count);
	printf("%s  [post-recv MPI_Status src:%i count:%i] \n", greeting, status.MPI_SOURCE, count);

	MPI_Barrier(MPI_COMM_WORLD);
	printf("post-Barrier %i\n", my_rank);
	}//Isend Irecv

	//Another Isend
	if (true){
		//
		char greeting[MAX_STRING], recv_greeting[MAX_STRING];
		int left, right;
		right = (my_rank + 1) % comm_sz;
    	left = my_rank - 1;
    	if (left < 0){
    			left = comm_sz - 1;}
    	sprintf(greeting, "process %d to process %d\n", my_rank, right);
		sprintf(recv_greeting, "");
		int recv_data_sz =  (strlen(greeting)+1);
	    MPI_Irecv(recv_greeting, recv_data_sz, MPI_CHAR, left, 123, MPI_COMM_WORLD, &recv_request);
	    
	    MPI_Isend(greeting, strlen(greeting)+1, MPI_CHAR, right, 123, MPI_COMM_WORLD, &send_request);
	    printf("%i waiting for recv  len=%i\n", my_rank, strlen(greeting)+1);
	    MPI_Wait(&recv_request, &status);
	    printf("%i waiting for sendlen=%i\n", my_rank, strlen(greeting)+1);
	    MPI_Wait(&send_request, &status);
	    printf("%s  [post-recv MPI_Status src:%i count:%i] \n", recv_greeting, status.MPI_SOURCE, count);
	}

	//Trapezoid
	{
		//Trap
		double a = 0.0, b = 3.0, h, local_a, local_b;
		double local_int, total_int;
		int source, n = 1024, local_n;;
		//Function prototypes
		double Trapezoid(double, double, int, double);
		h = (b-a)/n; /* h is the same for all processes */
		local_n = n/comm_sz; /* So is the number of trapezoids */

		local_a = a + my_rank*local_n*h;
		local_b = local_a + local_n*h;
		local_int = Trapezoid(local_a, local_b, local_n, h);

		if (my_rank != 0) {
				MPI_Send(&local_int, 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
		} else {
				total_int = local_int;
				for (source = 1; source < comm_sz; source++){
					MPI_Recv(&local_int, 1, MPI_DOUBLE, source, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				total_int += local_int;
				}//for
		}//else
		if (my_rank == 0) {
			printf("With n = %d trapezoids, our estimate\n", n);
			printf("of the integral from %f to %f = %.15e\n", a, b, total_int);
		}//if
		MPI_Barrier(MPI_COMM_WORLD);
		printf("post-Barrier %i\n", my_rank);
	}//trapezoid
	MPI_Barrier(MPI_COMM_WORLD);
	{// calculate pi
		{double PI25DT = 3.141592653589793238462643;
	  	double mypi, pi, h, sum, x;
	  	int i, num_intervals;
	  	bool already_run = false;
	  	while (1) {
		    if (my_rank == 0) {
		        num_intervals=100;//number of intervals
		    	//printf("Enter the number of intervals: (0 quits) ");
	         	//scanf("%d",&num_intervals);
		    }
		    //printf("Before BroadCast: num_intervals = %i, my_rank: %i\n", num_intervals, my_rank);
		    MPI_Bcast(&num_intervals, 1, MPI_INT, 0, MPI_COMM_WORLD);
		    //printf("After Broadcast: num_intervals = %i, my_rank: %i\n", num_intervals, my_rank);
		    if (already_run == true)
		        break;
		    else {
		         h   = 1.0 / (double) num_intervals;
		         sum = 0.0;
		         for (i = my_rank + 1; i <= num_intervals; i += comm_sz) {
		             x = h * ((double)i - 0.5);
		             sum += (4.0 / (1.0 + x*x));
		         }
		         mypi = h * sum;
		         MPI_Allreduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
		         if (my_rank == 0)
		             printf("pi is approximately %.16f, Error is %.16f\n",
		                    pi, fabs(pi - PI25DT));
		    }
		    already_run = true;
	  	}//while
	  	};
	  	MPI_Barrier(MPI_COMM_WORLD);
		printf("post-Barrier %i\n", my_rank);
	}

	MPI_Finalize();
 return 0;
} // main //

//Taken from Pacheco ch3
double Trapezoid(
	double 	left_endpt /* in*/,
	double 	right_endpt /* in*/,
	int 	trap_count  /* in*/,//local_n in the calling function
	double 	base_len  /* in*/) {
	double 	estimate, x;
	int i;
	//Function prototypes
	double f(double);

	estimate = (f(left_endpt) + f(right_endpt))/2.0;
	for (i = 1; i <= trap_count-1; i++) {
	x = left_endpt + base_len;
	estimate += f(x);
	}
	estimate = estimate*base_len;

	return estimate;
}//Trapezoid

double f(double x) {
    double return_val;
    /* Calculate f(x). */
    /* Store calculation in return_val. */

    //http://imaginingarchimedes.blogspot.com/2012/08/odd-and-even-functions-oddly-interesting.html
    return_val = pow(x, 1.80);//*math.sin(x*PI/180);//http://www.cplusplus.com/reference/cmath/sin/
    return return_val;
} // f