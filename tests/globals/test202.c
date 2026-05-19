struct S {
    int x;
    int y;
};

struct S s = { .y = 2, .x = 40 };

int main() {
    return s.x + s.y;
}
