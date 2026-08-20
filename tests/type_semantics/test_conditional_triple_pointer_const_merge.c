int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int ***ppp = &pp;
	int **const *cppp = &pp;
	int **const *merged = 1 ? ppp : cppp;

	return ***merged == 42 ? 0 : 1;
}
