int
main(void)
{
    const char *p = __func__;

#if __STDC_VERSION__ >= 201112L
    if (_Generic(__func__, char *: 42, default: 0) != 42)
        return 1;
#endif

    if (sizeof __func__ != 5)
        return 2;
    if (sizeof(__func__) != 5)
        return 3;
    if (&__func__[0] != p)
        return 4;
    if (__func__[0] != 'm')
        return 5;
    if (__func__[1] != 'a')
        return 6;
    if (__func__[2] != 'i')
        return 7;
    if (__func__[3] != 'n')
        return 8;
    if (__func__[4] != '\0')
        return 9;
    return 42;
}
