static int
f(x)
int x;
{
	return x;
}

int
main(void)
{
	int (*p)() = f;
	int (**pp)() = &p;
	return _Generic(pp, int (**)(): 1, int (**)(int): 2, default: 3);
}
