int main(void)
{
    if (sizeof(int) != 4)
        return 1;
    if (sizeof(long) != 8)
        return 2;
    if (sizeof(unsigned long) != 8)
        return 3;
    return 42;
}
