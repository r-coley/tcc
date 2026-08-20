static unsigned long add_ul(unsigned long a, unsigned long b)
{
    return a + b;
}

static unsigned long id_ul(unsigned long v)
{
    return v;
}

int main(void)
{
    unsigned long v;

    v = add_ul(0x100000000UL, 5UL);
    v = id_ul(v);
    if ((v >> 32) != 1)
        return 1;
    if ((v & 15UL) != 5)
        return 2;
    return 42;
}
