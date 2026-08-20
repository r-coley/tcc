struct Pair {
    int a;
    int b;
};

static struct Pair table[3] = {
    { 1, 10 },
    { 2, 20 },
    { 3, 30 }
};

int sum_global_pairs(void) {
    return table[0].a + table[1].b + table[2].a;
}

int sum_local_pairs(void) {
    struct Pair local[2];

    local[0].a = 4;
    local[0].b = 40;
    local[1].a = 5;
    local[1].b = 50;

    return local[0].b + local[1].a;
}

int main(void) {
    if (sum_global_pairs() != 24)
        return 1;
    if (sum_local_pairs() != 45)
        return 2;
    return 0;
}
