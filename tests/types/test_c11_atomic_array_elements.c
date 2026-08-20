int
main(void)
{
	_Atomic int values[3] = { 1, 3, 5 };
	_Atomic int *mid = &values[1];

	values[0] = 7;
	if (values[0] != 7)
		return 1;
	if (*mid != 3)
		return 2;

	*mid = 11;
	if (values[1] != 11)
		return 3;

	values[2] = values[0] + values[1];
	if (values[2] != 18)
		return 4;

	return 42;
}
