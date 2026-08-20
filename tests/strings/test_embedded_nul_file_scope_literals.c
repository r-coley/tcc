static const char array_literal[] = "ab\0cd";
static const char *pointer_literal = "xy\0z";

int
main(void)
{
    if (sizeof(array_literal) != 6)
        return 1;
    if (array_literal[0] != 'a' || array_literal[1] != 'b' ||
        array_literal[2] != 0 || array_literal[3] != 'c' ||
        array_literal[4] != 'd' || array_literal[5] != 0)
        return 2;

    if (pointer_literal[0] != 'x' || pointer_literal[1] != 'y' ||
        pointer_literal[2] != 0 || pointer_literal[3] != 'z' ||
        pointer_literal[4] != 0)
        return 3;

    return 42;
}
