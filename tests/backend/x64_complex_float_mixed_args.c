int
x64_complex_float_mix(int left, _Complex float value, int right)
{
    return left + (int)(float)value + right;
}
