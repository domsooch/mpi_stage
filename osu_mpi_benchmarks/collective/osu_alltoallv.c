#define BENCHMARK "OSU MPI%s All-to-Allv Personalized Exchange Latency Test"
/*
 * Copyright (C) 2002-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University. 
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include "osu_coll.h"

int main(int argc, char *argv[])
{
    int i = 0, rank = 0, size, numprocs, disp;
    int  skip;
    double latency=0.0, t_start = 0.0, t_stop = 0.0;
    double timer=0.0;
    double avg_time = 0.0, max_time = 0.0, min_time = 0.0;
    char *sendbuf=NULL, *recvbuf=NULL;
    int *rdispls=NULL, *recvcounts=NULL, *sdispls=NULL, *sendcounts=NULL;
    int po_ret;
    size_t bufsize;

    set_header(HEADER);
    set_benchmark_name("osu_alltoallv");
    enable_accel_support();
    po_ret = process_options(argc, argv);

    if (po_okay == po_ret && none != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

    switch (po_ret) {
        case po_bad_usage:
            print_bad_usage_message(rank);
            MPI_Finalize();
            exit(EXIT_FAILURE);
        case po_help_message:
            print_help_message(rank);
            MPI_Finalize();
            exit(EXIT_SUCCESS);
        case po_version_message:
            print_version_message(rank);
            MPI_Finalize();
            exit(EXIT_SUCCESS);
        case po_okay:
            break;
    }

    if(numprocs < 2) {
        if (rank == 0) {
            fprintf(stderr, "This test requires at least two processes\n");
        }

        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

    if ((options.max_message_size * numprocs) > options.max_mem_limit) {
        options.max_message_size = options.max_mem_limit / numprocs;
    }

    if (allocate_buffer((void**)&recvcounts, numprocs*sizeof(int), none)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    if (allocate_buffer((void**)&sendcounts, numprocs*sizeof(int), none)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    if (allocate_buffer((void**)&rdispls, numprocs*sizeof(int), none)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    if (allocate_buffer((void**)&sdispls, numprocs*sizeof(int), none)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    bufsize = options.max_message_size * numprocs;
    if (allocate_buffer((void**)&sendbuf, bufsize, options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    set_buffer(sendbuf, options.accel, 1, bufsize);

    if (allocate_buffer((void**)&recvbuf, bufsize,
                options.accel)) {
        fprintf(stderr, "Could Not Allocate Memory [rank %d]\n", rank);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    set_buffer(recvbuf, options.accel, 0, bufsize);

    print_preamble(rank);

    MPI_Barrier(MPI_COMM_WORLD);

    for(size = 1; size <= options.max_message_size; size *= 2) {
        if(size > LARGE_MESSAGE_SIZE) {
            skip = SKIP_LARGE;
            options.iterations = options.iterations_large;
        }

        else {
            skip = SKIP;
            
        }

        disp =0;
        for ( i = 0; i < numprocs; i++) {
            recvcounts[i] = size;
            sendcounts[i] = size;
            rdispls[i] = disp;
            sdispls[i] = disp;
            disp += size;

        }

        MPI_Barrier(MPI_COMM_WORLD);

        timer=0.0;
        for(i = 0; i < options.iterations + skip; i++) {
            t_start = MPI_Wtime();

              MPI_Alltoallv(sendbuf, sendcounts, sdispls, MPI_CHAR, recvbuf, recvcounts, rdispls, MPI_CHAR,
                      MPI_COMM_WORLD);

            t_stop = MPI_Wtime();

            if(i>=skip)
            {
                timer+=t_stop-t_start;
            }
            MPI_Barrier(MPI_COMM_WORLD);  
        }

        latency = (double)(timer * 1e6) / options.iterations;

        MPI_Reduce(&latency, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0,
                MPI_COMM_WORLD);
        MPI_Reduce(&latency, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0,
                MPI_COMM_WORLD);
        MPI_Reduce(&latency, &avg_time, 1, MPI_DOUBLE, MPI_SUM, 0,
                MPI_COMM_WORLD);
        avg_time = avg_time/numprocs;

        print_stats(rank, size, avg_time, min_time, max_time);

        MPI_Barrier(MPI_COMM_WORLD);
    }

    free_buffer(rdispls, none);
    free_buffer(sdispls, none);
    free_buffer(recvcounts, none);
    free_buffer(sendcounts, none);
    free_buffer(sendbuf, options.accel);
    free_buffer(recvbuf, options.accel);

    MPI_Finalize();

    if (none != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}

/* vi: set sw=4 sts=4 tw=80: */
