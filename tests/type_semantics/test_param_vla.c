static int
sum_param_vla(int n, int values[n])
{
	int i;
	int sum = 0;

	for (i = 0; i < n; i = i + 1)
		sum = sum + values[i];

	return sum;
}

int
main(void)
{
	int values[4];

	values[0] = 3;
	values[1] = 5;
	values[2] = 7;
	values[3] = 11;

	return sum_param_vla(4, values) - 26;
}
