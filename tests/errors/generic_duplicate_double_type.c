int
main(void)
{
    double d = 1.0;

    return _Generic(d, double: 1, double: 2, default: 3);
}
