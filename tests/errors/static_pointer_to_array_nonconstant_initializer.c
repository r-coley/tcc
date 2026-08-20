void
f(int *p)
{
	static int (*a)[3] = (int (*)[3])p;
}
