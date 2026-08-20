typedef struct Pair {
	int a;
	int b;
} Pair;

Pair g;

int
main(void)
{
	Pair p = {40, 2};

	g = p;
	return g.a + g.b - 42;
}
