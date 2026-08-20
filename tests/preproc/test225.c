#define STRV(...) \
    #__VA_ARGS__

int main() {
    char *s;

    s = STRV(hello);

    return s[0];
}
