int
main(void)
{
	void *vp = 0;
	const volatile int *cvp = 0;
	void *bad = 1 ? vp : cvp;
	return bad != 0;
}
