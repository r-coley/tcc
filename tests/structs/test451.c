/* Three-field struct: verify all offsets correct */
struct RGB { char r; char g; char b; };
int main() {
    struct RGB c;
    c.r = 10;
    c.g = 20;
    c.b = 12;
    return c.r + c.g + c.b;
}
