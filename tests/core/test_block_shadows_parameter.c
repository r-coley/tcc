static int
f(int value)
{
    int total = value;

    {
        int value = 2;
        total += value;
    }

    total += value;
    return total;
}

int
main(void)
{
    return f(20);
}
