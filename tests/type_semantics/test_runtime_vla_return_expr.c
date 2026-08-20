int
sum_tail(int n)
{
	int values[n];
	int i;

	for (i = 0; i < n; i = i + 1)
		values[i] = i + 1;

	return values[n - 1] + values[0];
}

int
main(void)
{
	return sum_tail(5) - 6;
}
