#define STR(x) #x
#define FIRST2(a,b) STR(a ## b)

int main() {
    char *s;

    s = FIRST2(x, y);

    return s[0];
}
