/* Return type: pointer (8 bytes on arm64) — must use ldr x0 not ldr w0 */
static int g = 77;
int *ret_ptr(void) { return &g; }
int main() { return *ret_ptr(); }
