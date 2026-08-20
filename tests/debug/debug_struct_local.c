struct TccDbgPoint {
    int x;
    int y;
};

int debug_struct_probe(void)
{
    struct TccDbgPoint p;
    p.x = 19;
    p.y = 23;
    return p.x + p.y;
}

int main(void)
{
    return debug_struct_probe() == 42 ? 0 : 1;
}
