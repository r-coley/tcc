struct pair {
    int a;
    int b;
};

struct pair make_pair_with_args(int x, int y, int z)
{
    struct pair p;

    p.a = x + y;
    p.b = y + z;

    return p;
}

int use_pair_with_args(int seed)
{
    struct pair p;

    p = make_pair_with_args(seed, 20, 30);

    return p.a + p.b;
}

int main(void)
{
    return use_pair_with_args(10);
}
