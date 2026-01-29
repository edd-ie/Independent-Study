#include "comms.hpp"

std::string broadcast(int rank, int caster, const std::string &msg)
{
    // Step 1: broadcast the size from caster to everyone
    int size = msg.size();
    MPI_Bcast(&size, 1, MPI_INT, caster, MPI_COMM_WORLD);

    // Step 2: allocate buffer on all ranks
    std::vector<char> buffer(size + 1);

    // Step 3: root copies its message into the buffer
    if (rank == caster)
        std::copy(msg.begin(), msg.end(), buffer.begin());

    // Step 4: broadcast the raw bytes
    MPI_Bcast(buffer.data(), size + 1, MPI_CHAR, caster, MPI_COMM_WORLD);

    // Step 5: reconstruct string
    return std::string(buffer.data());
}
