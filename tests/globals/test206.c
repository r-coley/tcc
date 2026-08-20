struct S { int x; int y; };

struct S arr[] = {
    { 1, 2 },
    { 40, 2 }
};

int main() { return arr[1].x + arr[1].y; }
