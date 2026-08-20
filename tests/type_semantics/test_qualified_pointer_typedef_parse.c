typedef const int *cint_ptr;

int main(void)
{
    const int value = 42;
    cint_ptr p = &value;
    const int * volatile q = p;
    const int * restrict r = q;
    return *r;
}
