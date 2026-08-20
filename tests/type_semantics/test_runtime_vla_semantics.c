/* Runtime VLAs should behave like ordinary automatic arrays. */

static int
sum_runtime_vla(int n)
{
	int values[n][4];
	int i;
	int j;
	int sum = 0;

	for (i = 0; i < n; i = i + 1) {
		for (j = 0; j < 4; j = j + 1) {
			values[i][j] = i + j + 1;
			sum = sum + values[i][j];
		}
	}

	return sum;
}

int
main(void)
{
	return sum_runtime_vla(3) - 42;
}
