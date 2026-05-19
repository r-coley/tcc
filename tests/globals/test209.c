struct S { char c; int i; };

struct S arr[] = {
    { .i = 41, .c = 1 }
};

int main() { return arr[0].c + arr[0].i; }
