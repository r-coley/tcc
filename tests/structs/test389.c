struct Point {
    int x;
    int y;
};

int main() {
    struct Point a = { 0, 0 };
    struct Point b = { 10, 9 };
    struct Point c = { 0, 0 };
    struct Point d = { 20, 32 };

    int z = (a = b).x + (c = d).y;
    return z;
}
