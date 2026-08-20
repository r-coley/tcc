struct S {
	int n;
	int values[];
};

struct S s = { 1, { 2, 3 } };

int
main(void)
{
	return 0;
}
