int
takes_nested(const volatile int **cvpp)
{
	return cvpp != 0;
}

int
main(void)
{
	int value = 0;
	int *p = &value;
	int **pp = &p;

	return takes_nested(pp);
}
