/* Array of structs — offset into second element must be sizeof(struct) */
struct Pt { int x; int y; };
static struct Pt pts[3];
int main() {
    pts[0].x = 1;  pts[0].y = 2;
    pts[1].x = 10; pts[1].y = 20;
    pts[2].x = 5;  pts[2].y = 7;
    return pts[1].x + pts[1].y + pts[2].x + pts[2].y;  /* 10+20+5+7=42 */
}
