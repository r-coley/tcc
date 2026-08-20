struct S { int x; int y; };

struct S arr[2];

int main() {
    arr[1].x = 40;
    arr[1].y = 2;
    return arr[1].x + arr[1].y;
}
