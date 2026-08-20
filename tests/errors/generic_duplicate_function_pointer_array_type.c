typedef int (*fn_t)(int);
typedef fn_t fnarr_t[2];

int
f(int x)
{
    return x;
}

int
main(void)
{
    fn_t arr[2] = {f, f};

    return _Generic(&arr,
                    int (*(*)[2])(int): 1,
                    fnarr_t *: 2,
                    default: 3);
}
