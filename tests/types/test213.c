typedef struct Forward Forward;

struct Forward {
    int x;
};

int main() {
    struct Forward f;
    f.x = 42;
    return f.x;
}
