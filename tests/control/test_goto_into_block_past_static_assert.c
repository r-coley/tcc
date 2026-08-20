#if __STDC_VERSION__ < 201112L
int
main(void)
{
    return 42;
}
#else
int
main(void)
{
    goto inside;
    {
        _Static_assert(1, "ok");
inside:
        return 42;
    }
}
#endif
