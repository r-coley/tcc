#define JOIN(a,b) a ## b

int main() {
    int xy;

    xy = 55;

    return JOIN(x, y);
}
