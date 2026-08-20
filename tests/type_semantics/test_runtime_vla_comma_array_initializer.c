int
main(void)
{
	int n = 3;
	int values[n], fixed[2] = {4, 5};

	values[0] = fixed[0];
	values[1] = fixed[1];
	values[2] = 0;

	return values[0] + values[1] - 9;
}
