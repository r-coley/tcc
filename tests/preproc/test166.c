#define A 1

#if defined(A)
#undef A
#endif

#if defined(A)
int main() {
    return 0;
}
#else
int main() {
    return 8;
}
#endif
