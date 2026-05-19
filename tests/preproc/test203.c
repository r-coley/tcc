#define VALUE 7
#define ID(x) x
#define MUL(a,b) ((a)*(b))

int main() {
    return MUL(ID(VALUE), 6);
}
