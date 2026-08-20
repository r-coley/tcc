int inc(int x) { return x + 1; }
int dec(int x) { return x - 1; }

int main(void) {
    int (*fp)(int);
    fp = inc;
    return fp(41) + dec(1);
}
