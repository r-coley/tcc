struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 40, 2 };
    int z = (a = b).x;

    return z + a.y;
}
