#define ADD3(a,b,c) ((a) + (b) + (c))
#define WRAP(x) ((x) * 2)
#define PICK(a,b) (b)

int main(void) {
    int a = ADD3(10,
                 20,
                 12);
    int b = WRAP(ADD3(1,
                      2,
                      3));
    int c = PICK("not a macro call ADD3(",
                 5);

    if (a != 42)
        return 1;
    if (b != 12)
        return 2;
    if (c != 5)
        return 3;

    return 42;
}
