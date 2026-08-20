int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	const int *cp = &value;
	const int **cpp = &cp;

	return (1 ? pp : cpp) != 0;
}
