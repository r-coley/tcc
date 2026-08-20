static long double add(long double a, long double b)
{
    return a + b;
}

int main(void)
{
    long double a = 1.25L;
    long double b = 2.5L;
    long double c = add(a, b);
    double d = (double)c;

    if (!(c > 3.74L && c < 3.76L))
        return 1;
    if (!(d > 3.74 && d < 3.76))
        return 2;
    return 42;
}
