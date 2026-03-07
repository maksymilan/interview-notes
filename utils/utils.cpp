#include "random.h"
#include <random>

int random_integer(int lower_bound, int upper_bound) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(lower_bound, upper_bound);
    return  distrib(gen);
}