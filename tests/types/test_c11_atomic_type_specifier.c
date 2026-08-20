int
main(void)
{
	_Atomic(int) value = 5;
	_Atomic(int) other = 7;
	_Atomic(int) *ptr = &value;

	if (value != 5)
		return 1;
	if (*ptr != 5)
		return 2;

	value = other;
	if (value != 7)
		return 3;

	*ptr = 42;
	if (value != 42)
		return 4;

	return 42;
}
