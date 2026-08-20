struct Item {
    char c;
    short s;
    int i;
};

struct Item item = { 3, 1000, 39 };

int main(void) {
    if (item.c + item.s + item.i != 1042)
        return 1;
    return 0;
}
