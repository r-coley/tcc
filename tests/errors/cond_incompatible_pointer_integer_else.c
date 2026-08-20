void
g(void)
{
	int *p = 0;
	int *a = 1 ? p : 7;
	(void)a;
}
