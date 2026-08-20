int
main(void)
{
	const volatile int *cvp = 0;
	void *vp = cvp;
	return vp != 0;
}
