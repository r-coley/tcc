int
main(void)
{
    int x = 3;
    void *p = &x;

    return *(void *)p;
}
