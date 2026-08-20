int main(void)
{
#ifdef __APPLE__
    char *argv[3];
    argv[0] = "dsymutil";
    argv[1] = "out";
    argv[2] = 0;
    return argv[0][0] == 'd' && argv[1][0] == 'o' && argv[2] == 0 ? 42 : 1;
#else
    return 1;
#endif
}
