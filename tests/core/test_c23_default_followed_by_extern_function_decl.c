int
main(void)
{
    switch (1) {
    case 0:
        return 1;
    default:
        extern int helper(void);
        return 42;
    }
}
