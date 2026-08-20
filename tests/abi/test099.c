/* Variadic snprintf with multiple format args */
int snprintf(char *buf, unsigned long n, const char *fmt, ...);

int main() {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", 42);
    return (buf[0] - '0') * 10 + (buf[1] - '0');
}
