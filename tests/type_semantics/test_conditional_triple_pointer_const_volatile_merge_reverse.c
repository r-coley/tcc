int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int ***ppp = &pp;
	int **const volatile *cvppp = &pp;
	int **const volatile *merged = 1 ? cvppp : ppp;

	return ***merged == 42 ? 0 : 1;
}
