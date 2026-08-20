struct Inner {
    char c;
    int n;
};

int sum_inner(struct Inner i) {
    return i.c + i.n;
}

int main() {
    return sum_inner((struct Inner){ .n = 40, .c = 2 });
}
