typedef union Value {
    int i;
    char c;
} Value;

int main() {
    Value v;
    v.i = 42;
    return v.i;
}
