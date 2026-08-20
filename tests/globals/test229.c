#include <stdio.h>

static const char *get_reg(int i)
{
    static const char *regs[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
    return regs[i];
}

int main(void)
{
    if (get_reg(0)[0] != 'r') return 1;
    if (get_reg(1)[1] != 's') return 2;
    if (get_reg(4)[1] != '8') return 3;
    if (get_reg(5)[1] != '9') return 4;
    return 0;
}
