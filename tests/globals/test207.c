struct S { int x; int y; };

struct S arr[3] = {
    { 40, 2 }
};

int main() { return arr[0].x + arr[0].y + arr[1].x + arr[2].y; }
