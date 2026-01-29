#if !defined(START_IMPLEMENTATION)
#define START_IMPLEMENTATION

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

void commTest(const int rank, const int num_proc, int * const  recv, const int tag, MPI_Status *const msg)
{
    const int rank_next = (rank + 1) % num_proc;
    const int rank_prev = rank == 0 ? num_proc - 1 : rank - 1;

    srandom(rank + 33);
    int value = random() % 100;

    if (rank % 2 == 0)
    {
        printf("Proc %d sending %d to %d\n\n", rank, value, rank_next);

        MPI_Send(&value, 1, MPI_INT, rank_next, tag, MPI_COMM_WORLD);
    }
    else
    {
        MPI_Recv(recv, 1, MPI_INT, rank_prev, tag, MPI_COMM_WORLD, msg);
        printf("Proc %d received %d from %d\n\n", rank, *recv, rank_prev);
    }
}

#endif // START_IMPLEMENTATION
