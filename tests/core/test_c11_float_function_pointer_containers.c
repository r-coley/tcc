typedef float (*FloatBinop)(float, float);
typedef double (*DoubleBinop)(double, double);

struct FloatOps {
    FloatBinop add;
    FloatBinop sub;
};

struct DoubleOps {
    DoubleBinop add;
    DoubleBinop sub;
};

float addf(float a, float b)
{
    return a + b;
}

float subf(float a, float b)
{
    return a - b;
}

double addd(double a, double b)
{
    return a + b;
}

double subd(double a, double b)
{
    return a - b;
}

FloatBinop float_ops[] = { addf, subf };
DoubleBinop double_ops[] = { [1] = subd, [0] = addd };

struct FloatOps sf = { addf, subf };
struct DoubleOps sd = { .sub = subd, .add = addd };

int main(void)
{
    FloatBinop local_float_ops[] = { addf, subf };
    DoubleBinop local_double_ops[] = { [1] = subd, [0] = addd };
    struct FloatOps lsf = { addf, subf };
    struct DoubleOps lsd = { .sub = subd, .add = addd };

    if (float_ops[0](19.5f, 22.5f) != 42.0f) return 1;
    if (float_ops[1](50.0f, 8.0f) != 42.0f) return 2;
    if (double_ops[0](19.5, 22.5) != 42.0) return 3;
    if (double_ops[1](50.0, 8.0) != 42.0) return 4;

    if (local_float_ops[0](19.5f, 22.5f) != 42.0f) return 5;
    if (local_float_ops[1](50.0f, 8.0f) != 42.0f) return 6;
    if (local_double_ops[0](19.5, 22.5) != 42.0) return 7;
    if (local_double_ops[1](50.0, 8.0) != 42.0) return 8;

    if (sf.add(19.5f, 22.5f) != 42.0f) return 9;
    if (sf.sub(50.0f, 8.0f) != 42.0f) return 10;
    if (sd.add(19.5, 22.5) != 42.0) return 11;
    if (sd.sub(50.0, 8.0) != 42.0) return 12;

    if (lsf.add(19.5f, 22.5f) != 42.0f) return 13;
    if (lsf.sub(50.0f, 8.0f) != 42.0f) return 14;
    if (lsd.add(19.5, 22.5) != 42.0) return 15;
    if (lsd.sub(50.0, 8.0) != 42.0) return 16;

    return 42;
}
