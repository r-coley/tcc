typedef int (*fn_t)(int);

int call(fn_t fn, int value);

int
call(int (*const fn)(int), int value)
{
    return fn(value);
}

int
id_int(int x)
{
    return x;
}

int
main(void)
{
    return call(id_int, 42) - 42;
}
