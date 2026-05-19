int main() {
    char c;
    char *p;

    c = 250;
    p = &c;
    (*p) += 10;

    return c;
}
