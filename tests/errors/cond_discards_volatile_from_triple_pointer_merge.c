int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	int ***ppp = &pp;
	int **volatile *vppp = &pp;
	int ***bad = 1 ? ppp : vppp;

	return bad != 0;
}
