int main(void) {
    int x;
    int *p;

    x = 40;
    p = &x;
    (*p)++;

    return x;
}
