struct S {
	int x;
};

int
main(void)
{
	struct S s;
	const struct S *p = &s;
	p->x = 2;
	return 0;
}
