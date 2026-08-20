int f(int *p);

int
f(int *const p)
{
    return *p;
}

int
main(void)
{
    int x = 42;
    return f(&x) - 42;
}
