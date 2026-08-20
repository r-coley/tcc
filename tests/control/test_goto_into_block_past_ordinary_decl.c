int
main(void)
{
    int result = 1;

    goto inside;

    {
        int skipped = 0;
        (void)skipped;

inside:
        result = 42;
    }

    return result;
}
