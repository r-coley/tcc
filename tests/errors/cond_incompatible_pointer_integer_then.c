void
g(void)
{
	int *p = 0;
	int *a = 1 ? 9 : p;
	(void)a;
}
