struct S {
    int x;
    int y;
};

struct S s = { .y = 42 };

int main() {
    return s.y;
}
