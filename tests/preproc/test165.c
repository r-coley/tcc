#define A 10
#undef A

#if defined(A)
int main() {
    return 0;
}
#else
int main() {
    return 7;
}
#endif
