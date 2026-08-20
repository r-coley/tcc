static void *stub_dlsym(void *p, const char *zSym)
{
    (void)p;
    (void)zSym;
    return 0;
}

int main(void)
{
    void (*(*x)(void *, const char *))(void);
    x = (void (*(*)(void *, const char *))(void))stub_dlsym;
    return x ? 1 : 42;
}
