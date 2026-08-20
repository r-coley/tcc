int
main(void)
{
	int values[4];
	int (*pa)[4] = &values;

	values[0] = 1;
	values[1] = 2;
	values[2] = 7;
	values[3] = 4;

	return (*pa)[2] - 7;
}
