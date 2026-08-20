char *id(char *);

char *id(char *p) {
    return p;
}

int main() {
    char *s;

    s = id("abc");

    return s[1];
}
