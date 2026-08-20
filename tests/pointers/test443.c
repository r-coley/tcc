int main() {
    int values[3];

    values[0] = 10;
    values[1] = 20;
    values[2] = 42;

    int *p;
    p = &values[0];

    return p[2];
}
