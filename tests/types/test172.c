enum Color {
    RED,
    GREEN
};

typedef enum Color Color;

int main() {
    return sizeof(enum Color) + sizeof(Color);
}
