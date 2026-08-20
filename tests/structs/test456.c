struct TccPtrPtrPoint {
    int x;
    int y;
};

int f(void)
{
    struct TccPtrPtrPoint p;
    struct TccPtrPtrPoint *q;
    struct TccPtrPtrPoint **qq;

    p.x = 19;
    p.y = 23;
    q = &p;
    qq = &q;

    return (*qq)->x + (*qq)->y;
}

int main(void)
{
    return f();
}
