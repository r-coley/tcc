int
foo(void)
{
	return 1;
}

int
main(void)
{
	{
		typedef int foo;
		foo value = 42;
		return value;
	}
}
