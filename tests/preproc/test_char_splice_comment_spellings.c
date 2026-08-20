int
main(void)
{
    int block_open_0 = '/';
    int block_open_1 = '\
*';
    int block_close_0 = '*';
    int block_close_1 = '\
/';
    int line_open_0 = '/';
    int line_open_1 = '\
/';

    return (block_open_0 == '/' &&
            block_open_1 == '*' &&
            block_close_0 == '*' &&
            block_close_1 == '/' &&
            line_open_0 == '/' &&
            line_open_1 == '/') ? 42 : 1;
}
