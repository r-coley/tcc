extern int f(int), g(int *);

int
f(int x)
{
	return x;
}

int
g(int *p)
{
	return *p;
}

int
main(void)
{
	return g(1);
}
