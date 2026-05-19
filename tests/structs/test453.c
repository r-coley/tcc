/* Nested struct field offsets */
struct Inner { int x; int y; };
struct Outer { char tag; struct Inner in; };
int main() {
    struct Outer o;
    o.tag = 1;
    o.in.x = 20;
    o.in.y = 22;
    return o.in.x + o.in.y;  /* 41 ... let's use 20+22=42 */
}
