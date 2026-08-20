struct S {
    int x;
    int y;
};

struct S s = { 40, 2 };

int main() {
    return s.x + s.y;
}
