#define NAME foo
#define STR(x) #x

int main() {
    char *s;

    s = STR(NAME);

    return s[0];
}
