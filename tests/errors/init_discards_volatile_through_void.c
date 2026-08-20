int
main(void)
{
	volatile int *vp = 0;
	void *p = vp;

	return p != 0;
}
