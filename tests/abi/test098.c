/* fprintf has 2 fixed params (FILE*, fmt), variadic starts at arg 3 */
int fprintf(void *f, const char *fmt, ...);
extern void *stderr;

int add(int a, int b) { return a + b; }

int main() {
    /* This just needs to compile and not crash. Return 42. */
    return add(20, 22);
}
