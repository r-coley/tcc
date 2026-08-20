static int callee(int a, int b, int c, int d,
                  int e, int f, int g, int h)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8;
}

static int caller(int a, int b, int c, int d, int e,
                  int f, int g, int h, int i, int j)
{
    int local0 = i + j;
    int local1 = a - b;
    int local2 = c + d + e;

    return callee(
        local0,
        local1,
        local2,
        f,
        g,
        h,
        i,
        j
    );
}

int main(void)
{
    int r = caller(10, 3, 4, 5, 6, 7, 8, 9, 11, 12);
    return r - 377;
}
