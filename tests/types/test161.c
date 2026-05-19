union Value {
    int i;
    char c;
};

int main() {
    union Value v;
    v.i = 42;
    return sizeof(v);
}
