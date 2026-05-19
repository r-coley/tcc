#define A 1

#if 0 || defined(A)
int main() {
    return 5;
}
#else
int main() {
    return 0;
}
#endif
