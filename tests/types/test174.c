int main() {
    int values[4];
    int *p;

    p = &values[0];

    return sizeof(values) + sizeof(p) + sizeof(p[0]);
}
