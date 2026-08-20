/* Regression: embedded NUL bytes in char[] initialisers must not truncate local storage. */
int main(void)
{
    char s[] = "xy\0z";
    return (sizeof(s) == 5 && s[0] == 'x' && s[1] == 'y' &&
            s[2] == 0 && s[3] == 'z' && s[4] == 0) ? 42 : 1;
}
