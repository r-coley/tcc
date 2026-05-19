struct S {
    int x;
    int y;
};

struct S s = { .x = 40, .y = 2 };

int main() {
    return s.x + s.y;
}
