struct Pair {
    int a;
    int b;
};

int main(void) {
    struct Pair items[3];

    items[0].a = 1;
    items[0].b = 2;
    items[1].a = 10;
    items[1].b = 20;
    items[2].a = 3;
    items[2].b = 6;

    return items[1].a + items[1].b + items[2].b + items[0].b + items[0].a + items[2].a;
}
