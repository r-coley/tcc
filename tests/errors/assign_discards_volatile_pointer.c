int
main(void)
{
	int *p;
	volatile int *vp = 0;

	p = vp;
	return p != 0;
}
