int main(void)
{
#ifdef __APPLE__
    int value = 42;
#else
    int value = 1;
#endif
    return value;
}
