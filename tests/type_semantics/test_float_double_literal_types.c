int main(void)
{
    if (sizeof(1.0f) != 4)
        return 1;
    if (sizeof(1.0) != 8)
        return 2;
    return 42;
}
