#define CAT3(a,b,c) \
    a ## b ## c

int main() {
    int xyz;

    xyz = 42;

    return CAT3(x, y, z);
}
