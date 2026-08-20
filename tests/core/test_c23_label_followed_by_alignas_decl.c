int
main(void)
{
label:
    alignas(16) int x = 42;
    return x;
}
