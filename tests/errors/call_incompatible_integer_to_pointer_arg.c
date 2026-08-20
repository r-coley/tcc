void
takes_ptr(int *p)
{
	(void)p;
}

void
g(void)
{
	takes_ptr(11);
}
