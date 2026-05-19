#define A foo
#define B bar
#define CAT(a,b) a ## b
#define foobar 88

int main() {
    return CAT(A, B);
}
