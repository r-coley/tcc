static int dbg_square(int x)
{
    int y = x * x;
    return y;
}

static int dbg_mix(int a, int b)
{
    int left = dbg_square(a);
    int right = dbg_square(b);
    return left + right;
}

int main(void)
{
    int value = dbg_mix(3, 4);
    if (value != 25)
        return 1;
    return 0;
}
