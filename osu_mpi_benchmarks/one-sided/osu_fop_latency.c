#define BENCHMARK "OSU MPI_Fetch_and_op latency Test"
/*
 * Copyright (C) 2003-2014 the Network-Based Computing Laboratory
 * (NBCL), The Ohio State University.            
 *
 * Contact: Dr. D. K. Panda (panda@cse.ohio-state.edu)
 *
 * For detailed copyright and licensing information, please refer to the
 * copyright file COPYRIGHT in the top level OMB directory.
 */

#include <inttypes.h>
#include "osu_1sc.h"

#define MAX_MSG_SIZE sizeof(uint64_t)
#define MYBUFSIZE (MAX_MSG_SIZE + MAX_ALIGNMENT)

#ifdef PACKAGE_VERSION
#   define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#   define HEADER "# " BENCHMARK "\n"
#endif

int     skip = 10;
int     loop = 500;
double  t_start = 0.0, t_end = 0.0;
char    sbuf_original[MYBUFSIZE];
char    rbuf_original[MYBUFSIZE];
char    tbuf_original[MYBUFSIZE];
uint64_t *sbuf=NULL, *rbuf=NULL, *tbuf=NULL;

void print_header (int, WINDOW, SYNC); 
void print_latency (int, int);
void run_fop_with_lock (int, WINDOW);
void run_fop_with_fence (int, WINDOW);
void run_fop_with_lock_all (int, WINDOW);
void run_fop_with_flush (int, WINDOW);
void run_fop_with_flush_local (int, WINDOW);
void run_fop_with_pscw (int, WINDOW);

int main (int argc, char *argv[])
{
    SYNC        sync_type=FLUSH; 
    int         rank,nprocs;
   
    int         page_size;
    int         po_ret = po_okay;
    WINDOW      win_type=WIN_ALLOCATE;

    po_ret = process_options(argc, argv, &win_type, &sync_type, all_sync);

    if (po_okay == po_ret && none != options.accel) {
        if (init_accel()) {
            fprintf(stderr, "Error initializing device\n");
            exit(EXIT_FAILURE);
        }
    }
 
    MPI_CHECK(MPI_Init(&argc, &argv));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &nprocs));
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    if (0 == rank) {
        switch (po_ret) {
            case po_cuda_not_avail:
                fprintf(stderr, "CUDA support not enabled.  Please recompile "
                        "benchmark with CUDA support.\n");
                break;
            case po_openacc_not_avail:
                fprintf(stderr, "OPENACC support not enabled.  Please "
                        "recompile benchmark with OPENACC support.\n");
                break;
            case po_bad_usage:
            case po_help_message:
                usage(all_sync);
                break;
        }

    }

    switch (po_ret) {
        case po_cuda_not_avail:
        case po_openacc_not_avail:
        case po_bad_usage:
            MPI_Finalize();
            exit(EXIT_FAILURE);
        case po_help_message:
            MPI_Finalize();
            exit(EXIT_SUCCESS);
        case po_okay:
            break;
    }

    if(nprocs != 2) {
        if(rank == 0) {
            fprintf(stderr, "This test requires exactly two processes\n");
        }

        MPI_CHECK(MPI_Finalize());

        return EXIT_FAILURE;
    }

    print_header(rank, win_type, sync_type);

    switch (sync_type){
        case LOCK:
            run_fop_with_lock(rank, win_type);
            break;
        case LOCK_ALL:
            run_fop_with_lock_all(rank, win_type);
            break;
        case PSCW:
            run_fop_with_pscw(rank, win_type);
            break;
        case FENCE: 
            run_fop_with_fence(rank, win_type);
            break;
        case FLUSH_LOCAL:
            run_fop_with_flush_local(rank, win_type);
            break;
        default: 
            run_fop_with_flush(rank, win_type);
            break;
    }

    MPI_CHECK(MPI_Finalize());


    if (none != options.accel) {
        if (cleanup_accel()) {
            fprintf(stderr, "Error cleaning up device\n");
            exit(EXIT_FAILURE);
        }
    }
    return EXIT_SUCCESS;
}

void print_header (int rank, WINDOW win, SYNC sync)
{
    if(rank == 0) {
        switch (options.accel) {
            case cuda:
                printf(HEADER, "-CUDA");
                break;
            case openacc:
                printf(HEADER, "-OPENACC");
                break;
            default:
                printf(HEADER, "");
                break;
        }
        fprintf(stdout, "# Window creation: %s\n",
                win_info[win]);
        fprintf(stdout, "# Synchronization: %s\n",
                sync_info[sync]);

        switch (options.accel) {
            case cuda:
            case openacc:
                printf("# Rank 0 Memory on %s and Rank 1 Memory on %s\n",
                        'D' == options.rank0 ? "DEVICE (D)" : "HOST (H)",
                        'D' == options.rank1 ? "DEVICE (D)" : "HOST (H)");
            default:
                fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
                fflush(stdout);
        }
    }
}

void print_latency(int rank, int size)
{
    if (rank == 0) {
        fprintf(stdout, "%-*d%*.*f\n", 10, size, FIELD_WIDTH,
                FLOAT_PRECISION, (t_end - t_start) * 1.0e6 / loop);
        fflush(stdout);
    }
}

/*Run FOP with flush local*/
void run_fop_with_flush_local (int rank, WINDOW type)
{
    int i;
    MPI_Win     win;

    MPI_Aint disp = 0;

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    allocate_atomic_memory(rank, sbuf_original, rbuf_original,
                tbuf_original, NULL, (char **)&sbuf, (char **)&rbuf,
                (char **)&tbuf, NULL, (char **)&rbuf,  MAX_MSG_SIZE, type, &win);

    if(rank == 0) {
        if (type == WIN_DYNAMIC) {
            disp = disp_remote;
        }

        MPI_CHECK(MPI_Win_lock(MPI_LOCK_SHARED, 1, 0, win));
        for (i = 0; i < skip + loop; i++) {
            if (i == skip) {
                t_start = MPI_Wtime ();
            }
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 1, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_flush_local(1, win));
        }
        t_end = MPI_Wtime ();
        MPI_CHECK(MPI_Win_unlock(1, win));
    }                

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    print_latency(rank, 8);

    free_atomic_memory (sbuf, rbuf, tbuf, NULL, win, rank);
}

/*Run FOP with flush */
void run_fop_with_flush (int rank, WINDOW type)
{
    int i;
    MPI_Aint disp = 0;
    MPI_Win     win;

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    allocate_atomic_memory(rank, sbuf_original, rbuf_original,
                tbuf_original, NULL, (char **)&sbuf, (char **)&rbuf,
                (char **)&tbuf, NULL, (char **)&rbuf,  MAX_MSG_SIZE, type, &win);

    if(rank == 0) {
        if (type == WIN_DYNAMIC) {
            disp = disp_remote;
        }
        MPI_CHECK(MPI_Win_lock(MPI_LOCK_SHARED, 1, 0, win));
        for (i = 0; i < skip + loop; i++) {
            if (i == skip) {
                t_start = MPI_Wtime ();
            }
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 1, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_flush(1, win));
        }
        t_end = MPI_Wtime ();
        MPI_CHECK(MPI_Win_unlock(1, win));
    }                

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    print_latency(rank, 8);

    free_atomic_memory (sbuf, rbuf, tbuf, NULL, win, rank);
}

/*Run FOP with Lock_all/unlock_all */
void run_fop_with_lock_all (int rank, WINDOW type)
{
    int i;
    MPI_Aint disp = 0;
    MPI_Win     win;

    allocate_atomic_memory(rank, sbuf_original, rbuf_original,
                tbuf_original, NULL, (char **)&sbuf, (char **)&rbuf,
                (char **)&tbuf, NULL, (char **)&rbuf,  MAX_MSG_SIZE, type, &win);

    if(rank == 0) {
        if (type == WIN_DYNAMIC) {
            disp = disp_remote;
        }

        for (i = 0; i < skip + loop; i++) {
            if (i == skip) {
                t_start = MPI_Wtime ();
            }
            MPI_CHECK(MPI_Win_lock_all(0, win));
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 1, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_unlock_all(win));
        }
        t_end = MPI_Wtime ();
    }                

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    print_latency(rank, 8);

    free_atomic_memory (sbuf, rbuf, tbuf, NULL, win, rank);
}

/*Run FOP with Lock/unlock */
void run_fop_with_lock(int rank, WINDOW type)
{
    int i;
    MPI_Aint disp = 0;
    MPI_Win     win;

    allocate_atomic_memory(rank, sbuf_original, rbuf_original,
                tbuf_original, NULL, (char **)&sbuf, (char **)&rbuf,
                (char **)&tbuf, NULL, (char **)&rbuf,  MAX_MSG_SIZE, type, &win);

    if(rank == 0) {
        if (type == WIN_DYNAMIC) {
            disp = disp_remote;
        }

        for (i = 0; i < skip + loop; i++) {
            if (i == skip) {
                t_start = MPI_Wtime ();
            }
            MPI_CHECK(MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 1, 0, win));
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 1, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_unlock(1, win));
        }
        t_end = MPI_Wtime ();
    }                

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    print_latency(rank, 8);

    free_atomic_memory (sbuf, rbuf, tbuf, NULL, win, rank);
}

/*Run FOP with Fence */
void run_fop_with_fence(int rank, WINDOW type)
{
    int i;
    MPI_Aint disp = 0;
    MPI_Win     win;

    allocate_atomic_memory(rank, sbuf_original, rbuf_original,
                tbuf_original, NULL, (char **)&sbuf, (char **)&rbuf,
                (char **)&tbuf, NULL, (char **)&rbuf,  MAX_MSG_SIZE, type, &win);

    if (type == WIN_DYNAMIC) {
        disp = disp_remote;
    }
    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    if(rank == 0) {

        for (i = 0; i < skip + loop; i++) {
            if (i == skip) {
                t_start = MPI_Wtime ();
            }
            MPI_CHECK(MPI_Win_fence(0, win));
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 1, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_fence(0, win));
            MPI_CHECK(MPI_Win_fence(0, win));
        }
        t_end = MPI_Wtime ();
    } else {
        for (i = 0; i < skip + loop; i++) {
            MPI_CHECK(MPI_Win_fence(0, win));
            MPI_CHECK(MPI_Win_fence(0, win));
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 0, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_fence(0, win));
        }
    }

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    if (rank == 0) {
        fprintf(stdout, "%-*d%*.*f\n", 10, 8, FIELD_WIDTH,
                FLOAT_PRECISION, (t_end - t_start) * 1.0e6 / loop / 2);
        fflush(stdout);
    }

    free_atomic_memory (sbuf, rbuf, tbuf, NULL, win, rank);
}

/*Run FOP with Post/Start/Complete/Wait */
void run_fop_with_pscw(int rank, WINDOW type)
{
    int destrank, i;
    MPI_Aint disp = 0;
    MPI_Win     win;

    MPI_Group       comm_group, group;
    MPI_CHECK(MPI_Comm_group(MPI_COMM_WORLD, &comm_group));

    allocate_atomic_memory(rank, sbuf_original, rbuf_original, 
                tbuf_original, NULL, (char **)&sbuf, (char **)&rbuf, 
                (char **)&tbuf, NULL, (char **)&rbuf,  MAX_MSG_SIZE, type, &win);

    if (type == WIN_DYNAMIC) {
        disp = disp_remote;
    }

    if (rank == 0) {
        destrank = 1;

        MPI_CHECK(MPI_Group_incl(comm_group, 1, &destrank, &group));
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

        for (i = 0; i < skip + loop; i++) {
            MPI_CHECK(MPI_Win_start (group, 0, win));

            if (i == skip) {
                t_start = MPI_Wtime ();
            }

            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 1, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_complete(win));
            MPI_CHECK(MPI_Win_post(group, 0, win));
            MPI_CHECK(MPI_Win_wait(win));
        }

        t_end = MPI_Wtime ();
    } else {
        /* rank=1 */
        destrank = 0;

        MPI_CHECK(MPI_Group_incl(comm_group, 1, &destrank, &group));
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

        for (i = 0; i < skip + loop; i++) {
            MPI_CHECK(MPI_Win_post(group, 0, win));
            MPI_CHECK(MPI_Win_wait(win));
            MPI_CHECK(MPI_Win_start(group, 0, win));
            MPI_CHECK(MPI_Fetch_and_op(sbuf, tbuf, MPI_LONG_LONG, 0, disp, MPI_SUM, win));
            MPI_CHECK(MPI_Win_complete(win));
        }
    }

    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));

    if (rank == 0) {
        fprintf(stdout, "%-*d%*.*f\n", 10, 8, FIELD_WIDTH,
                FLOAT_PRECISION, (t_end - t_start) * 1.0e6 / loop / 2);
        fflush(stdout);
    }

    MPI_CHECK(MPI_Group_free(&group));
    MPI_CHECK(MPI_Group_free(&comm_group));

    free_atomic_memory (sbuf, rbuf, tbuf, NULL, win, rank);
}
/* vi: set sw=4 sts=4 tw=80: */
