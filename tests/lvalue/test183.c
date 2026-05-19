int main() {
    int x;
    int *p;

    x = 7;
    p = &x;
    (*p)++;

    return x;
}
