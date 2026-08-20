int main(void)
{
    char chars[64];
    int ints[3];
    int value = 0;

    value += _Alignof(char) == 1;
    value += _Alignof(short) == 2;
    value += _Alignof(int) == 4;
    value += _Alignof(long) == 8;
    value += _Alignof(double) == 8;
    value += _Alignof(int *) == 8;
    value += _Alignof(chars) == 1;
    value += _Alignof(ints) == 4;
    value += _Alignof(value) == 4;

    return value == 9 ? 42 : value;
}
