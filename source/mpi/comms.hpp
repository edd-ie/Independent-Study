#pragma once
#include <mpi.h>
#include <print>
#include <vector>
#include <string>
#include <algorithm>

std::string broadcast(int rank, int caster, const std::string &msg);

void test_comms(int argc, char **argv)
{
    using std::println;
    using std::string;

    int num_proc, rank;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &num_proc);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    println("{}: rank of ({}) ", rank, num_proc);

    string msg;
    int caster = num_proc / 2;

    if (rank == caster)
        msg = "hello";

    string ans = broadcast(rank, caster, msg);

    if (rank != caster)
        println("{}: Received message: {}", rank, ans);
    else
        println("{}: Sending message: {}", rank, ans);

    MPI_Finalize();
}