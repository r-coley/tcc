typedef int *IntPtr;

int main() {
    int x;
    IntPtr p;

    x = 0;
    p = (IntPtr)&x;
    *p = 42;

    return x;
}
