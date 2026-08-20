#define TYPEOF(x) _Generic((x), \
    int (*)[3]: 5, \
    const int (*)[3]: 6, \
    int (*)[2][3]: 7, \
    int *: 3, \
    const int *: 4, \
    default: 99)

int
main(void)
{
    int a[3] = {1, 2, 3};
    int matrix[2][3] = {0};
    const int ca[3] = {4, 5, 6};

    if (TYPEOF(&a) != 5)
        return 1;
    if (TYPEOF(a) != 3)
        return 2;
    if (TYPEOF(&ca) != 6)
        return 3;
    if (TYPEOF(ca) != 4)
        return 4;
    if (TYPEOF(&matrix) != 7)
        return 5;
    if (TYPEOF(matrix) != 5)
        return 6;
    if (TYPEOF(matrix[0]) != 3)
        return 7;

    return 42;
}
