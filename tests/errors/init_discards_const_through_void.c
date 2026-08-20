int
main(void)
{
	const int *cp = 0;
	void *vp = cp;
	return vp != 0;
}
