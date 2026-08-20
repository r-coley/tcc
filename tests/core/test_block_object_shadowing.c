int value = 40;

int
main(void)
{
    int total = 0;

    total += value;

    {
        int value = 1;
        total += value;

        {
            int value = 2;
            total += value;
        }

        total += value;
    }

    total += value;
    return total - 42;
}
