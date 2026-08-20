_Complex float
x64_complex_float_parts(float real, float imag)
{
    _Complex float value = (_Complex float)real;
    ((float *)&value)[1] = imag;
    return value;
}
