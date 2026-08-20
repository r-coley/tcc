#define A 63
#define B (A + 1)
#define C ((B / 2) * 2)

char g[B];
char h[C];

int main(void) {
    char l[B];

    if (sizeof(g) != 64)
        return 1;
    if (sizeof(h) != 64)
        return 2;
    if (sizeof(l) != 64)
        return 3;

    return 42;
}
