int
main(void)
{
	int x = 0;
	return _Generic(x, int: 1, const int: 2, default: 3);
}
