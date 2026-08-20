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
	return _Generic(p, int (*)(): 1, int (*)(int): 2, default: 3);
}
