int main() {
    int x;
    int y;
    int *p;

    x = 8;
    p = &x;
    y = (*p)++;

    return y * 10 + x;
}
