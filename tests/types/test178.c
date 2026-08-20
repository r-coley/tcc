int main() {
    int x;
    char *p;

    x = 42;
    p = (char *)&x;

    return *p;
}
