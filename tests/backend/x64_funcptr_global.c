int add3(int x) {
    return x + 3;
}

int (*gfp)(int) = add3;

int main(void) {
    return gfp(39);
}
