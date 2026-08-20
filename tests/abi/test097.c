/* Variadic function: int arg in variadic position goes on stack ([sp]),
   not in x1 register. Tests printf-style calling convention. */
int printf(const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);

int main() {
    char buf[32];
    sprintf(buf, "%d", 42);
    /* buf should contain "42" — return first digit minus '0' + 4 = 4+4 = ... */
    /* simpler: return buf[0] - '0' + buf[1] - '0' */
    return (buf[0] - '0') * 10 + (buf[1] - '0');
}
