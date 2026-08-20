int
f(void)
{
	return 0;
}

int
main(void)
{
	extern int f(void) = 1;
	return 0;
}
