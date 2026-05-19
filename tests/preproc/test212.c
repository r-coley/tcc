#define STR(x) #x

int main() {
    char *s;

    s = STR(abc);

    return s[1];
}
