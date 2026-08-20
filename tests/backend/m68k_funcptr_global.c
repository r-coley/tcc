int add3(int x) {
    return x + 3;
}

int (*gfp)(int) = add3;

int main(void) {
    if (gfp(39) != 42)
        return 1;

    return 0;
}
