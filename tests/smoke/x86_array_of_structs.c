struct Item {
    int a;
    int b;
};

int main(void) {
    struct Item items[3];

    items[0].a = 1;
    items[0].b = 2;
    items[1].a = 10;
    items[1].b = 20;
    items[2].a = 100;
    items[2].b = 200;

    return items[1].a + items[2].b;
}
