int
main(void)
{
	_Atomic int value = 7;
	_Atomic int next = 9;
	_Atomic int *ptr = &value;

	if (value != 7)
		return 1;
	if (*ptr != 7)
		return 2;

	value = next;
	if (value != 9)
		return 3;

	*ptr = 11;
	if (value != 11)
		return 4;

	return 42;
}
