#define TYPEOF(x) _Generic((x), \
    unsigned long: 1, \
    unsigned long long: 2, \
    int: 3, \
    default: 99)

int
main(void)
{
    int x = 0;

    if (TYPEOF(sizeof x) != 1)
        return 1;
    if (TYPEOF(sizeof(int)) != 1)
        return 2;
    if (TYPEOF(_Alignof(int)) != 1)
        return 3;
    if (TYPEOF(_Alignof(long long)) != 1)
        return 4;

    if (sizeof ++x != sizeof(int))
        return 5;
    if (x != 0)
        return 6;

    return 42;
}
