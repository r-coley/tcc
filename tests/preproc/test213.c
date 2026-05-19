#define STR(x) #x

int main() {
    char *s;

    s = STR(1 + 2);

    return s[0];
}
