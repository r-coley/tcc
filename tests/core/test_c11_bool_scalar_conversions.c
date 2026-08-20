int
main(void)
{
	_Bool b;
	int value = 0;
	int *ptr = &value;

	b = -3;
	if (b != 1)
		return 1;
	b = 0;
	if (b != 0)
		return 2;
	b = ptr;
	if (b != 1)
		return 3;
	b = (int *)0;
	if (b != 0)
		return 4;
	b = 0.25f;
	if (b != 1)
		return 5;
	b = 0.0;
	if (b != 0)
		return 6;
	if (sizeof(+b) != sizeof(int))
		return 7;

	return 42;
}
