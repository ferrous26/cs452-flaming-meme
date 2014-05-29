
#include <rand.h>

static int _seed;

void srand(int seed) {
    _seed = seed;
}

int rand() {
    // This is the code the has been used extensively by Prof. Buhr
    // for CS246 and also CS343 since the actual distrubution of the
    // randomness doesn't matter much for any of out purposes it should be
    // fine
    //
    // Wont work properly with interrupts, fine for now
    //
    _seed = 36969 * (_seed & 65535) + (_seed >> 16); // scramble bits
    return _seed;
}

