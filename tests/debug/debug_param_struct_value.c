struct TccDbgPoint {
    int x;
    int y;
};

int debug_param_struct_value_probe(struct TccDbgPoint p, int scale)
{
    return p.x + p.y + scale;
}

int main(void)
{
    struct TccDbgPoint p;

    p.x = 10;
    p.y = 20;

    return debug_param_struct_value_probe(p, 12) == 42 ? 0 : 1;
}
