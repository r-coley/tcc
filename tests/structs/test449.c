/* Field offsets: char then int — int must be at offset 4 (aligned), not 1 */
struct S { char c; int i; };
int main() {
    struct S s;
    s.c = 0;
    s.i = 42;
    return s.i;
}
