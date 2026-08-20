struct Pair {
    int a;
    int b;
};

struct Pair items[2] = {
    { 10, 11 },
    { 20, 22 }
};

int main(void) {
    if (items[0].a + items[0].b + items[1].b != 43)
        return 1;

    return 0;
}
