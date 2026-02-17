#include <print>
#include "IOuring/cat_readv.hpp"

int test_cat(const int argc, char *const argv[]);

int main(int argc, char **argv)
{

    // Testing file reads to console using readv()
    test_cat(argc, argv);

    return 0;
}