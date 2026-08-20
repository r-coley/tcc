int main(void) {
    int x;
    int *p;

    x = 5;
    p = &x;
    (*p)++;

    return x;
}
