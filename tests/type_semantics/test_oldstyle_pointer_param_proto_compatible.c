int f(int *const p);

int
f(p)
int *p;
{
	return *p;
}

int
main(void)
{
	int value = 42;
	return f(&value) - 42;
}
