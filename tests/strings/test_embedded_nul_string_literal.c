/* Regression: embedded NUL bytes in string literals must survive AST/IR/data emission. */
int main(void)
{
    const char *s = "ab\0cd";
    return (s[0] == 'a' && s[1] == 'b' && s[2] == 0 &&
            s[3] == 'c' && s[4] == 'd' && s[5] == 0) ? 42 : 1;
}
