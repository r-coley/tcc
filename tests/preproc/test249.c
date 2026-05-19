#define A 1
#define B 0

#if defined(A) && !B
int value() { return 42; }
#else
int value() { return 0; }
#endif
