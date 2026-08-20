typedef int *intptr;

int main() {
    int values[2];
    values[0] = 1;
    values[1] = 42;

    intptr p;
    p = &values[0];

    return p[1];
}
