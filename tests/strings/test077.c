char *pick(int n) {
    if (n)
        return "yes";

    return "no";
}

int main() {
    char *s;

    s = pick(1);

    return s[0];
}
