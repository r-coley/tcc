char msg[] = "hello";
char *p = msg;

int main(void) {
    if (p[0] != 'h')
        return 1;
    if (p[4] != 'o')
        return 2;
    return 0;
}
