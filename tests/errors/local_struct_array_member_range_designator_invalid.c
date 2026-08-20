struct S {
	int values[3];
};

int
main(void)
{
	struct S s = { .values = { [2 ... 1] = 42 } };

	return s.values[1];
}
