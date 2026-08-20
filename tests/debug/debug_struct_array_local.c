struct TccDbgPoint {
    int x;
    int y;
};

int debug_struct_array_probe(void)
{
    struct TccDbgPoint pts[2];

    pts[0].x = 10;
    pts[0].y = 11;
    pts[1].x = 20;
    pts[1].y = 1;

    return pts[0].x + pts[0].y + pts[1].x + pts[1].y;
}

int main(void)
{
    return debug_struct_array_probe() == 42 ? 0 : 1;
}
