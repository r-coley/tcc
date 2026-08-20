int
main(void)
{
	void *vp;
	const volatile int *cvp = 0;

	vp = cvp;
	return 0;
}
