int main(void) {
    int values[3];

    values[0] = 10;
    values[1] = 20;
    values[2] = 12;

    if (values[0] + values[1] + values[2] != 42)
        return 1;

    return 0;
}
