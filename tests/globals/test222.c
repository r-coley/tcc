/* Regression: duplicate initialized global definitions must be rejected.
   This locks down the explicit-zero initializer tracking path. */
int duplicate_zero = 0;
int duplicate_zero = 0;
int main(void) { return duplicate_zero; }
