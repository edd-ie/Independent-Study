#pragma once
#include <mpi.h>
#include <print>
#include <vector>
#include <string>
#include <algorithm>

std::string broadcast(int rank, int caster, const std::string &msg);