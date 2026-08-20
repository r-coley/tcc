static int
classify(int x)
{
    switch (x) {
    case 0:
        return 40;
    default:
        return 1;
    case 2:
        return 42;
    }
}

int
main(void)
{
    if (classify(0) != 40)
        return 1;
    if (classify(2) != 42)
        return 2;
    if (classify(9) != 1)
        return 3;
    return 42;
}
