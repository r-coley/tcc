int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	int *const *cpp = &p;
	int **bad = 1 ? pp : cpp;

	return bad != 0;
}
