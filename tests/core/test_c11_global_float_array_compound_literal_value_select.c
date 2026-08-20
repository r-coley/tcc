static float gf = ((float[2]){ 1.25f, 2.5f })[1];
static double gd = ((double[2][2]){ { 1.0, 2.0 }, { 3.5, 4.5 } })[1][0];

int main(void)
{
    if (!(gf > 2.49f && gf < 2.51f))
        return 11;
    if (!(gd > 3.49 && gd < 3.51))
        return 12;
    return 42;
}
