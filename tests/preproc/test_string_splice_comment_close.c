int main(void) {
    const char *close_like = "ab*\
/cd";

    return (close_like[2] == '*' &&
            close_like[3] == '/') ? 42 : 1;
}
