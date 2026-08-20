struct TccDbgPoint {
    int x;
    int y;
};

int debug_struct_ptr_ptr_probe(void)
{
    struct TccDbgPoint p;
    struct TccDbgPoint *q;
    struct TccDbgPoint **qq;

    p.x = 19;
    p.y = 23;
    q = &p;
    qq = &q;

    return (*qq)->x + (*qq)->y;
}

int main(void)
{
    return debug_struct_ptr_ptr_probe() == 42 ? 0 : 1;
}
