int
main(void)
{
    if (1)
        alignas(16) int x = 1;
    return 0;
}
