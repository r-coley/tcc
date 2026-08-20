int main(void)
{
    float f;
    double d;

    if (sizeof(f) != 4)
        return 1;
    if (sizeof(d) != 8)
        return 2;
    if (_Alignof(float) != 4)
        return 3;
    if (_Alignof(double) != 8)
        return 4;
    if (_Alignof(d) != 8)
        return 5;
    return 42;
}
