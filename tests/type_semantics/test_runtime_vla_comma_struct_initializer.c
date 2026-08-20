typedef struct Pair {
	int a;
	int b;
} Pair;

int
main(void)
{
	int n = 1;
	Pair values[n], pair = {4, 5}, extra = {1, 2};

	return pair.a + pair.b + extra.b - 11;
}
