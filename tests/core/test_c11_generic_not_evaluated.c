int main(void)
{
	int x = 1;
	int y = 10;
	int r;

	r = _Generic(++x, int: 20, long: ++y, default: ++y);
	if (x != 1 || y != 10 || r != 20)
		return 1;

	r = _Generic(1L, int: ++x, long: 22, default: ++y);
	if (x != 1 || y != 10 || r != 22)
		return 2;

	return 42;
}
