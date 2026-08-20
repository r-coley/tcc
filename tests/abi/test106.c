/* Regression: x64 SysV calls must be stack-aligned even when all arguments
   are passed in registers; variadic integer-only calls must set %al to zero. */
int snprintf(char *buf, unsigned long n, const char *fmt, ...);

int main(void) {
    char buf[8];

    snprintf(buf, sizeof(buf), "%d", 42);
    return (buf[0] - '0') * 10 + (buf[1] - '0');
}
