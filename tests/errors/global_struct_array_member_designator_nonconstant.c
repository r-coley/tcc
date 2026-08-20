struct S {
	int values[3];
};

int hi = 2;

struct S s = { .values = { [0 ... hi] = 42 } };

int
main(void)
{
	return s.values[1];
}
