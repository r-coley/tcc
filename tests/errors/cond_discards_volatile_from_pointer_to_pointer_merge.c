int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	int *volatile *vpp = &p;
	int **bad = 1 ? pp : vpp;

	return bad != 0;
}
