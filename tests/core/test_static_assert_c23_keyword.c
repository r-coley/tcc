static_assert(sizeof(int) == 4, "int should stay 4 bytes");

int main(void)
{
    static_assert(sizeof(long) == 8);
    return 42;
}
