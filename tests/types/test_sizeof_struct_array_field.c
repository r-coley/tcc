/* Test: sizeof on array fields of local and global structs */
typedef struct {
    int x;
    char name[64];
    char buf[128];
    int y;
} S;

static S gs;  /* global */

int test_global_array_field(void) {
    return (int)sizeof(gs.name);   /* should be 64 */
}

int test_global_array_field2(void) {
    return (int)sizeof(gs.buf);    /* should be 128 */
}

int test_global_scalar_field(void) {
    return (int)sizeof(gs.x);     /* should be 4 */
}

int main(void) {
    S ls;  /* local */
    int a = (int)sizeof(ls.name);  /* should be 64 */
    int b = (int)sizeof(ls.buf);   /* should be 128 */
    int c = (int)sizeof(ls.x);    /* should be 4 */
    int d = test_global_array_field();
    int e = test_global_array_field2();
    int f = test_global_scalar_field();
    if (a != 64)  return 1;
    if (b != 128) return 2;
    if (c != 4)   return 3;
    if (d != 64)  return 4;
    if (e != 128) return 5;
    if (f != 4)   return 6;
    return 42;
}
