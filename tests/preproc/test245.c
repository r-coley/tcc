#define A 10
#define B 20

#if (A < B) && defined(A)
int main() { return 42; }
#else
int main() { return 1; }
#endif
