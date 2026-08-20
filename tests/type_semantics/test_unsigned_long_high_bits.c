int main(void)
{
    unsigned long v;

    v = 0x100000002UL;
    if ((v >> 32) != 1)
        return 1;
    if ((v & 15UL) != 2)
        return 2;
    return 42;
}
