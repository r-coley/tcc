struct TccDbgPoint {
    int x;
    int y;
};

int debug_struct_ptr_probe(void)
{
    struct TccDbgPoint p;
    struct TccDbgPoint *q;

    p.x = 19;
    p.y = 23;
    q = &p;

    return q->x + q->y;
}

int main(void)
{
    return debug_struct_ptr_probe() == 42 ? 0 : 1;
}
