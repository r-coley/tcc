static int
id(int value)
{
	return value;
}

int
main(void)
{
	int value = 42;
	int *p = &value;
	int (*fp)(int) = id;

	if (_Generic((const int *)p, const int *: 1, default: 0) != 1)
		return 1;
	if (_Generic((volatile void *)p, volatile void *: 1, default: 0) != 1)
		return 2;
	if (_Generic((int (*)(int))fp, int (*)(int): 1, default: 0) != 1)
		return 3;
	if (_Generic((int (*)(int, ...))fp, int (*)(int, ...): 1, default: 0) != 1)
		return 4;

	return ((int (*)(int))fp)(42);
}
