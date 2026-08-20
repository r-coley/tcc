typedef struct Pair {
	int a;
	int b;
} Pair;

int
main(void)
{
	int n = 1;
	int sum;
	Pair values[n], pair = {4, 5};

	values[0] = pair;
	sum = values[0].a + values[0].b;
	return sum - 9;
}
