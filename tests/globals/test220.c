/* Regression: an explicit zero initializer after a tentative definition
   must be tracked as a real initializer, not mistaken for another tentative
   definition. */
int explicit_zero;
int explicit_zero = 0;

int sentinel = 42;

int main(void) {
    return explicit_zero + sentinel;
}
