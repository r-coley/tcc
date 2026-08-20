struct S {
	int n;
	int values[];
};

int
main(void)
{
	struct S s = { 1, { 2, 3 } };
	return s.n;
}
