typedef struct Node Node;

struct Node {
    int value;
    Node *next;
};

int main() {
    struct Node n;
    n.value = 42;
    return n.value;
}
