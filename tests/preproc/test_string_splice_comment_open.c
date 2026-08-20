int main(void) {
    const char *block_like = "ab/\
*cd";
    const char *line_like = "xy/\
/zz";

    return (block_like[2] == '/' &&
            block_like[3] == '*' &&
            line_like[2] == '/' &&
            line_like[3] == '/') ? 42 : 1;
}
