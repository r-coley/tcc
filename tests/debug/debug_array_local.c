int debug_array_probe(void)
{
    int a[3];

    a[0] = 10;
    a[1] = 20;
    a[2] = 12;

    return a[0] + a[1] + a[2];
}

int main(void)
{
    return debug_array_probe() == 42 ? 0 : 1;
}
