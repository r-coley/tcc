struct pair {
    int a;
    int b;
};

struct pair make_pair_nested(int x, int y)
{
    struct pair p;

    p.a = x + 1;
    p.b = y + 2;

    return p;
}

int consume_pair_nested(struct pair p, int scale)
{
    return p.a * scale + p.b;
}

int main(void)
{
    return consume_pair_nested(make_pair_nested(10, 20), 3);
}
