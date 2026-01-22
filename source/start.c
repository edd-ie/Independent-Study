#include "start.h"
#define START_IMPLEMENTATION

int main(int argc, char **argv)
{
    int num_proc, rank, recv;
    int tag = 100;
    MPI_Status msg;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    printf("Proc: %d startup of %d\n", rank, num_proc);
    commTest(rank, num_proc, &recv, tag, &msg);

    MPI_Finalize();
    return EXIT_SUCCESS;
}