int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	int *const volatile *cvpp = &p;
	int **bad = 1 ? pp : cvpp;

	return bad != 0;
}
