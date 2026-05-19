struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 21, 1 };
    struct Point c = { 0, 0 };
    struct Point d = { 5, 21 };

    int z = (a = b).x + (c = d).y;
    return z;
}
