int x86_many_args_sum(int a,
                      int b,
                      int c,
                      int d,
                      int e,
                      int f,
                      int g,
                      int h,
                      int i,
                      int j)
{
    return a + b * 2 + c * 3 + d * 4 + e * 5 +
           f * 6 + g * 7 + h * 8 + i * 9 + j * 10;
}

int x86_many_args_chain(int seed)
{
    return x86_many_args_sum(seed,
                             2,
                             3,
                             4,
                             5,
                             6,
                             7,
                             8,
                             9,
                             10);
}

int main(void)
{
    return x86_many_args_chain(1);
}
