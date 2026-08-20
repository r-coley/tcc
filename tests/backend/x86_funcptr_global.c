int plus2(int x) { return x + 2; }
int times3(int x) { return x * 3; }

int (*gfp)(int) = plus2;

int main(void) {
    int a = gfp(40);
    gfp = times3;
    return a + gfp(0);
}
