int
main(void)
{
    float f = 1.0f;

    return _Generic(f, float: 1, const float: 2, default: 3);
}
