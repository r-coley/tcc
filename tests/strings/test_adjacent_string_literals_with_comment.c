int
main(void)
{
    const char *s =
        "he" /* adjacent literal boundary with comment */
        "llo";

    if (sizeof("he" /* join */ "llo") != 6)
        return 1;
    if (s[0] != 'h')
        return 2;
    if (s[1] != 'e')
        return 3;
    if (s[2] != 'l')
        return 4;
    if (s[3] != 'l')
        return 5;
    if (s[4] != 'o')
        return 6;
    if (s[5] != '\0')
        return 7;
    return 42;
}
