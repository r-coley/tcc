/* Regression: x64 integer/pointer arguments beyond the first six must be
   passed on the caller stack and loaded by the callee in the correct order. */

int pick7(int a, int b, int c, int d, int e, int f, int g) {
    return g;
}

int pick9(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    return i;
}

int sum9(int a, int b, int c, int d, int e, int f, int g, int h, int i) {
    return a + b + c + d + e + f + g + h + i;
}

int main(void) {
    return pick7(1, 2, 3, 4, 5, 6, 7)
         + pick9(1, 2, 3, 4, 5, 6, 7, 8, 9)
         + sum9(1, 2, 3, 4, 5, 6, 7, 8, 9);
}
