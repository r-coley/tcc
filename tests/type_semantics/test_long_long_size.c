int main(void)
{
    if (sizeof(long long) != 8)
        return 1;
    if (sizeof(unsigned long long) != 8)
        return 2;
    return 42;
}
