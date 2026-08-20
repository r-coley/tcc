struct Point {
    int x;
    int y;
};

int main() {
    struct Point p;
    struct Point *q;

    q = (struct Point *)&p;

    return 42;
}
