int
main(void)
{
    int x = 0;
    int *p = &x;

    return p & 1;
}
