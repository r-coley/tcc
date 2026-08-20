int
main(void)
{
    int n = 3;
    int a[n];

    return _Generic(&a, int (*)[n]: 1, default: 2);
}
