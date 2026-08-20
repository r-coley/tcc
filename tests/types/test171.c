union Value {
    int i;
    char c;
};

typedef union Value Value;

int main() {
    return sizeof(union Value) + sizeof(Value) + sizeof(Value *);
}
