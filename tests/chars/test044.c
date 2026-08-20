struct Mixed {
    char a;
    int b;
};

int main() {
    struct Mixed m;

    m.a = 65;
    m.b = 7;

    return sizeof(struct Mixed);
}
