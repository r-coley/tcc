#define TYPEOF(x) _Generic((x), \
    int (*(*)[2])(int): 7, \
    int (**)(int): 8, \
    int (*)(int): 9, \
    default: 99)

static int
f(int x)
{
    return x + 1;
}

static int
g(int x)
{
    return x + 2;
}

int
main(void)
{
    int (*arr[2])(int) = {f, g};

    if (TYPEOF(&arr) != 7)
        return 1;
    if (TYPEOF(arr) != 8)
        return 2;
    if (TYPEOF(arr[0]) != 9)
        return 3;
    if (arr[1](40) != 42)
        return 4;

    return 42;
}
