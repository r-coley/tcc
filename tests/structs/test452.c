/* Mixed sizes: char(1) + short(2) + int(4) + ptr(8) — verify alignment gaps */
typedef struct {
    char   a;   /* offset 0, size 1 */
    short  b;   /* offset 2, size 2 (1 byte gap) */
    int    c;   /* offset 4, size 4 */
    int   *d;   /* offset 8, size 8 */
} Mixed;
static int g = 5;
int main() {
    Mixed m;
    m.a = 1;
    m.b = 2;
    m.c = 7;
    m.d = &g;
    return m.a + m.b + m.c + *m.d + 27;
}
