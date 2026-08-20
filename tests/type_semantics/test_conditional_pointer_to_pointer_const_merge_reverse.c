int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int *const *cpp = &p;
	int *const *merged = 1 ? cpp : pp;

	return **merged == 42 ? 0 : 1;
}
