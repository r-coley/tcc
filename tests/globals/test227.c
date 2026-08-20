/* Regression: global pointer arrays may be initialized from string literals. */
static const char *regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };

static int streq(const char *a, const char *b)
{
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }
    return a[i] == b[i];
}

int main(void)
{
    if (!streq(regs[0], "rdi")) return 1;
    if (!streq(regs[1], "rsi")) return 2;
    if (!streq(regs[2], "rdx")) return 3;
    if (!streq(regs[3], "rcx")) return 4;
    if (!streq(regs[4], "r8")) return 5;
    if (!streq(regs[5], "r9")) return 6;
    return 42;
}
