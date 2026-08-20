struct Pair {
    int a;
    int b;
};

struct Pair pairs[3] = {
    { 1, 2 },
    { 10, 20 },
    { 3, 4 }
};

int main(void) {
    return pairs[1].a + pairs[1].b + pairs[2].a + pairs[2].b + 5;  /* 42 */
}
