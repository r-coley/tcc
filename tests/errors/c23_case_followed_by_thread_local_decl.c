int
main(void)
{
    switch (0) {
    case 0:
        thread_local int x = 1;
        return x;
    }
}
