int
main(void)
{
    int result = 1;

    goto inside;

    {
        union LocalUnionTag;

inside:
        result = 42;
    }

    return result;
}
