#define CAT(a,b) a ## b
#define foobar 42

int main() {
    return CAT(foo, bar);
}
