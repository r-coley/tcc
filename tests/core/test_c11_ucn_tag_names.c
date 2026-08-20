struct caf\u00E9 {
    int value;
};

union ni\u00F1o {
    int value;
};

enum jalape\u00F1o {
    jalape\u00F1o_ok = 42
};

int
main(void)
{
    struct caf\u00E9 a;
    union ni\u00F1o b;
    enum jalape\u00F1o c;

    a.value = 20;
    b.value = 2;
    c = jalape\u00F1o_ok;

    return a.value + b.value + (int)c - 22;
}
