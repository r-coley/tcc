struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 1, 41 };
    struct Point b = { .y = 41, .x = 1 };

    if (a == b)
        return 42;

    return 0;
}
