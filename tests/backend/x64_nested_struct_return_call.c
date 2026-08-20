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

int x64_nested_struct_return_call(int seed)
{
    struct pair tmp;

    tmp = make_pair_nested(seed, 20);

    return consume_pair_nested(tmp, 3);
}

int main(void)
{
    return x64_nested_struct_return_call(10);
}
