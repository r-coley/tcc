int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	int ***ppp = &pp;
	int **const volatile *cvppp = &pp;
	int ***bad = 1 ? ppp : cvppp;

	return bad != 0;
}
