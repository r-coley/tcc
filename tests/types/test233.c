#include <stdint.h>

/* Regression: 32-bit equality comparisons must not be widened in a way that
   makes int32_t -1 differ from the 32-bit constant 0xffffffff. */
int main(void) {
    int32_t x;

    x = 0;
    x = ~x;

    if (x != 0xffffffff)
        return 1;

    return 42;
}
