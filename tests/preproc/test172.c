#define A 1
#define B 1

#if defined(A) && defined(B)
int main() {
    return 6;
}
#else
int main() {
    return 0;
}
#endif
