int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;
	volatile int **vpp = pp;

	return vpp != 0;
}
