int f(const void *p)
{
    unsigned char const *z = p;
    return z ? 42 : 0;
}

int main(void)
{
    unsigned char value = 1;
    return f(&value);
}
