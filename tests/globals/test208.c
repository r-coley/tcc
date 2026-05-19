struct S { int x; int y; };

struct S arr[] = {
    { .y = 2, .x = 40 },
    { .x = 1, .y = 1 }
};

int main() { return arr[0].x + arr[0].y; }
