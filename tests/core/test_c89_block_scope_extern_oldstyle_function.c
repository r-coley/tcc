int
f()
{
	return 42;
}

int
main()
{
	extern int f();
	return f();
}
