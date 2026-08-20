struct Node {
    int value;
    int next;
};

int get_value(struct Node *n) {
    return n->value;
}

void set_next(struct Node *n, int v) {
    n->next = v;
}

int main(void) {
    struct Node n;

    n.value = 42;
    n.next = 0;

    set_next(&n, 17);

    if (get_value(&n) != 42)
        return 1;
    if (n.next != 17)
        return 2;

    return 0;
}
