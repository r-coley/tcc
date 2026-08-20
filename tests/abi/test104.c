/* Mixed int/char/pointer arguments and char return */
char first_char(const char *s) { return s[0]; }
int main() {
    return first_char("*") - '*' + 42;
}
