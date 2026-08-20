int
main(void)
{
	int value = 42;
	int *p = &value;
	int **pp = &p;
	int *volatile *vpp = &p;
	int *volatile *merged = 1 ? pp : vpp;

	return **merged == 42 ? 0 : 1;
}
