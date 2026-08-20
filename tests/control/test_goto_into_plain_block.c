int
main(void)
{
    int result = 1;

    goto inside;

    {
inside:
        result = 42;
    }

    return result;
}
