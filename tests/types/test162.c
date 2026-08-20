union Value {
    int i;
    char c;
};

typedef union Value Value;

int main() {
    Value v;
    v.i = 42;
    return v.i;
}
