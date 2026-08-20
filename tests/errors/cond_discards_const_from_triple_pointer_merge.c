int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	int ***ppp = &pp;
	int **const *cppp = &pp;
	int ***bad = 1 ? ppp : cppp;

	return bad != 0;
}
