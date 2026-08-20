int main(void)
{
    char chars[8];
    int x = 0;

    if (alignof(char) != 1)
        return 1;
    if (alignof(int) != 4)
        return 2;
    if (alignof(int *) != 8)
        return 3;
    if (alignof(chars) != 1)
        return 4;
    if (alignof(x) != 4)
        return 5;

    return 42;
}
