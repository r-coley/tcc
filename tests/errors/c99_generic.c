int main(void)
{
    int x = 1;
    return _Generic(x, int: 0, default: 1);
}
