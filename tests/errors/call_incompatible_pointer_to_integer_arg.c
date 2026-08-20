void
takes_int(int x)
{
	(void)x;
}

void
g(void)
{
	int *p = 0;
	takes_int(p);
}
