int main() {
    char buf[4];
    char *p;

    buf[0] = 65;
    buf[1] = 66;
    buf[2] = 67;
    buf[3] = 68;

    p = buf;

    return *(p + 3);
}
