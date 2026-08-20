int
main(void)
{
    switch (0) {
    default:
        thread_local int x = 1;
        return x;
    }
}
