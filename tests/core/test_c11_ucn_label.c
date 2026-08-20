int
main(void)
{
    int value = 0;

    goto etiq\u00FCeta;

valor_\u00E9rroneo:
    return 1;

etiq\u00FCeta:
    value = 42;
    if (value != 42)
        goto valor_\u00E9rroneo;
    return value;
}
