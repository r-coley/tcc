static int
value(void)
{
    return 40;
}

int
main(void)
{
    int total = value();

    {
        int value = 1;
        total += value;
    }

    return total + 1;
}
