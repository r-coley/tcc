/* Pointer field at correct 8-byte aligned offset */
struct WithPtr { char tag; int *p; };
static int val = 42;
int main() {
    struct WithPtr s;
    s.tag = 1;
    s.p = &val;
    return *s.p;
}
