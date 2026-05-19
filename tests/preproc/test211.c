#define STR(x) #x

int main() {
    char *s;

    s = STR(hello);

    return s[0];
}
