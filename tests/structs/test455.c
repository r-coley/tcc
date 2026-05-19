/* Struct passed by value — all fields correctly copied */
struct Vec3 { int x; int y; int z; };
int sum_vec(struct Vec3 v) { return v.x + v.y + v.z; }
int main() {
    struct Vec3 v;
    v.x = 10; v.y = 15; v.z = 17;
    return sum_vec(v);
}
