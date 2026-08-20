typedef int (*fn_t)(int);

int
f(int x)
{
    return x;
}

int
main(int n)
{
    fn_t arr[n];

    arr[0] = f;
    return _Generic(&arr,
                    int (*(*)[n])(int): 1,
                    default: 2);
}
