struct P {
    int x;
    char y;
};

typedef struct P P;

int main() {
    P p;

    p.x = 40;
    p.y = 2;

    return p.x + p.y;
}
