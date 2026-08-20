#define A 1

#if defined(A) && 1
int main() {
    return 4;
}
#else
int main() {
    return 0;
}
#endif
