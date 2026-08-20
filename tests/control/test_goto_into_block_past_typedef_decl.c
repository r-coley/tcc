int
main(void)
{
    int result = 1;

    goto inside;

    {
        typedef int local_int;
        local_int value = 0;
        (void)value;

inside:
        result = 42;
    }

    return result;
}
