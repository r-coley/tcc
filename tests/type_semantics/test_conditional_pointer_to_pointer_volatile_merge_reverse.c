int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int *volatile *vpp = &p;
	int *volatile *merged = 1 ? vpp : pp;

	return **merged == 42 ? 0 : 1;
}
