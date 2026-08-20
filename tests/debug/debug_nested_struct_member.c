struct TccDbgPoint {
    int x;
    int y;
};

struct TccDbgBox {
    struct TccDbgPoint origin;
    int scale;
};

int debug_nested_struct_member_probe(void)
{
    struct TccDbgBox box;

    box.origin.x = 10;
    box.origin.y = 20;
    box.scale = 12;

    return box.origin.x + box.origin.y + box.scale;
}

int main(void)
{
    return debug_nested_struct_member_probe() == 42 ? 0 : 1;
}
