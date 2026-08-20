int
main(void)
{
	void *vp = 0;
	const int *cp = 0;
	const void *merged = 1 ? vp : cp;

	return merged != 0;
}
