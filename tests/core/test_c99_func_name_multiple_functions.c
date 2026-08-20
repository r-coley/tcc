static int
helper(void)
{
    const char *p = __func__;

    if (sizeof __func__ != 7)
        return 1;
    if (&__func__[0] != p)
        return 2;
    if (__func__[0] != 'h')
        return 3;
    if (__func__[1] != 'e')
        return 4;
    if (__func__[2] != 'l')
        return 5;
    if (__func__[3] != 'p')
        return 6;
    if (__func__[4] != 'e')
        return 7;
    if (__func__[5] != 'r')
        return 8;
    if (__func__[6] != '\0')
        return 9;
    return 40;
}

int
main(void)
{
    if (__func__[0] != 'm')
        return 1;
    if (__func__[1] != 'a')
        return 2;
    if (__func__[2] != 'i')
        return 3;
    if (__func__[3] != 'n')
        return 4;
    if (__func__[4] != '\0')
        return 5;
    return helper() + 2;
}
