int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int ***ppp = &pp;
	int **volatile *vppp = &pp;
	int **volatile *merged = 1 ? vppp : ppp;

	return ***merged == 42 ? 0 : 1;
}
