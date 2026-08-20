int
main(void)
{
    static int values[4] = {42, 0, 0, 0};
    static int (*ptr)[4] = &values;

    return (*ptr)[0];
}
