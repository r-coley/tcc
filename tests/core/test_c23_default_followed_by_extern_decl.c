int
main(void)
{
    switch (0) {
    case 1:
        return 0;
    default:
        extern int x;
        return 42;
    }
}
