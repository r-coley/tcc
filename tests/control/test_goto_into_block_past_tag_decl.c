int
main(void)
{
    int result = 1;

    goto inside;

    {
        struct LocalTag;

inside:
        result = 42;
    }

    return result;
}
