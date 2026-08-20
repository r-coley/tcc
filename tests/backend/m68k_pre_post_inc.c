int main(void) {
    int x;
    int y;

    x = 10;
    y = x++;

    if (y != 10)
        return 1;

    if (x != 11)
        return 2;

    y = ++x;

    if (y != 12)
        return 3;

    if (x != 12)
        return 4;

    return 0;
}
