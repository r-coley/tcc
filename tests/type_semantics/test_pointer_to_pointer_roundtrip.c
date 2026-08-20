int main(void)
{
    int value;
    int *p;
    int **pp;

    value = 42;
    p = &value;
    pp = &p;
    return **pp;
}
