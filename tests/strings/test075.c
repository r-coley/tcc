char *get() {
    return "hello";
}

int main() {
    char *s;

    s = get();

    return s[1];
}
