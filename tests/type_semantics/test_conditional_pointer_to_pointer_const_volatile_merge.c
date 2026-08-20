int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int *const volatile *cvpp = &p;
	int *const volatile *merged = 1 ? pp : cvpp;

	return **merged == 42 ? 0 : 1;
}
