int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	const volatile int **cvpp = pp;

	return cvpp != 0;
}
