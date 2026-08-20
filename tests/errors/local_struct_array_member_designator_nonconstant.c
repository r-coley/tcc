struct S {
	int values[3];
};

int
main(void)
{
	int i = 1;
	struct S s = { .values = { [i] = 42 } };

	return s.values[1];
}
