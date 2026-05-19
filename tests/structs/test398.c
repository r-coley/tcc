struct Point { int x; int y; };

int main() {
    struct Point a = {0, 0};
    struct Point b = {40, 1};
    struct Point c = {0, 0};
    struct Point d = {2, 9};

    return (a = b).x | (c = d).x;
}
