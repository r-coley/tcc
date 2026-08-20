static int
classify(int x)
{
    switch (x) {
    default:
        return 1;
    case 0:
        return 42;
    case 7:
        return 2;
    }
}

int
main(void)
{
    if (classify(0) != 42)
        return 1;
    if (classify(7) != 2)
        return 2;
    if (classify(9) != 1)
        return 3;
    return 42;
}
