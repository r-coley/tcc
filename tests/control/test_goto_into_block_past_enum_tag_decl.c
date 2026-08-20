int
main(void)
{
    int result = 1;

    goto inside;

    {
        enum LocalEnumTag;

inside:
        result = 42;
    }

    return result;
}
