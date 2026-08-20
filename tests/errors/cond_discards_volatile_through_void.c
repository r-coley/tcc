int
main(void)
{
	void *vp = 0;
	volatile int *vip = 0;
	void *bad = 1 ? vp : vip;
	return bad != 0;
}
