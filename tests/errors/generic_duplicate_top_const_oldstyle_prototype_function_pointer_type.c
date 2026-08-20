static int
f(x)
int x;
{
	return x;
}

int
main(void)
{
	int (* const p)() = f;
	return _Generic(p, int (* const)(): 1, int (*)(int): 2, default: 3);
}
