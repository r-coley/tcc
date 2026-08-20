typedef unsigned long tcc_size_type;

int main(void)
{
    tcc_size_type n;

    if (sizeof(tcc_size_type) != 8)
        return 1;
    n = 0x10000002aUL;
    if ((n >> 32) != 1)
        return 2;
    if ((n & 255UL) != 42)
        return 3;
    return 42;
}
