int
main(void)
{
    goto inside;

    {
        int n = 4;
        int values[n];
        values[0] = 1;
inside:
        return values[0];
    }
}
