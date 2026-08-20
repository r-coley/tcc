struct S {
	int x;
};

int
main(void)
{
	const struct S s;
	s.x = 2;
	return 0;
}
