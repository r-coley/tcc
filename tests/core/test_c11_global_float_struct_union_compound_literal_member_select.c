struct S { int x; float y; };
union U { int i; double d; };

static float gs = ((struct S){ .x = 1, .y = 2.5f }).y;
static double gu = ((union U){ .d = 3.5 }).d;

int main(void)
{
    if (!(gs > 2.49f && gs < 2.51f))
        return 11;
    if (!(gu > 3.49 && gu < 3.51))
        return 12;
    return 42;
}
