#define JOIN3(a,b,c) a ## b ## c

int main() {
    int abc;

    abc = 66;

    return JOIN3(a, b, c);
}
