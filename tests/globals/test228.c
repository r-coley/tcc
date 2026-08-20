/* Regression test: function-local static pointer arrays initialized
   from string literals.

   This is distinct from file-scope/global pointer arrays.  The x64
   backend previously exposed a stage1 crash when a function-local
   static register-name table was self-compiled incorrectly. */

static const char *
get_reg(int i)
{
    static const char *regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
    return regs[i];
}

static int
streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int
main(void)
{
    if (!streq(get_reg(0), "rdi"))
        return 1;
    if (!streq(get_reg(1), "rsi"))
        return 2;
    if (!streq(get_reg(2), "rdx"))
        return 3;
    if (!streq(get_reg(3), "rcx"))
        return 4;
    if (!streq(get_reg(4), "r8"))
        return 5;
    if (!streq(get_reg(5), "r9"))
        return 6;
    return 0;
}
