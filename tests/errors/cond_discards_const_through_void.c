int
main(void)
{
	void *vp = 0;
	const int *cp = 0;
	void *bad = 1 ? vp : cp;
	return bad != 0;
}
