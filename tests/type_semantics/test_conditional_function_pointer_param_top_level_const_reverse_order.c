typedef int (*fn_t)(int);

int call(fn_t fn, int value);

int
call(int (*const fn)(int), int value)
{
    return fn(value);
}

static int
inc(int x)
{
    return x + 1;
}

static int
dec(int x)
{
    return x - 1;
}

int
main(void)
{
    fn_t a = inc;
    int (*plain)(int) = dec;
    return (1 ? a : plain)(41) - 42;
}
