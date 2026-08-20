int f(void) { return 1; }
int g(int x) { return x + 1; }

int main(void) {
    int (*pf)(void) = f;
    int (**ppf)(void) = &pf;
    int total = 0;

    total += _Generic(f, int (*)(void): 10, int (*)(int): 1, default: 100);
    total += _Generic(g, int (*)(void): 1, int (*)(int): 20, default: 100);
    total += _Generic(pf, int (*)(void): 30, int (**)(void): 1, default: 100);
    total += _Generic(ppf, int (*)(void): 1, int (**)(void): 40, default: 100);

    return total == 100 ? 42 : total;
}
