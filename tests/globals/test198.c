struct S {
    int x;
    int y;
};

struct S s;

int main() {
    s.x = 40;
    s.y = 2;
    return s.x + s.y;
}
