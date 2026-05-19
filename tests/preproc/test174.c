#define A 1
#define B 1

#if defined(A) && !defined(B)
int main() {
    return 0;
}
#elif 1
int main() {
    return 8;
}
#endif
