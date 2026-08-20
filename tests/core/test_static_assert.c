_Static_assert(sizeof(int) == 4, "int should stay 4 bytes");
_Static_assert(sizeof(long) == 8, "long should stay 8 bytes on LP64");

int main(void)
{
    _Static_assert(sizeof(char) == 1, "char should stay 1 byte");
    _Static_assert(sizeof(int *) == 8, "pointer should stay 8 bytes");
    return 42;
}
