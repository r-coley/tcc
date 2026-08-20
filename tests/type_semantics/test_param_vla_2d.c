static int
sum_param_vla_2d(int n, int values[n][4])
{
	int i;
	int j;
	int sum = 0;

	for (i = 0; i < n; i = i + 1) {
		for (j = 0; j < 4; j = j + 1)
			sum = sum + values[i][j];
	}

	return sum;
}

int
main(void)
{
	int values[3][4];
	int i;
	int j;
	int next = 1;

	for (i = 0; i < 3; i = i + 1) {
		for (j = 0; j < 4; j = j + 1) {
			values[i][j] = next;
			next = next + 1;
		}
	}

	return sum_param_vla_2d(3, values) - 78;
}
