extern inline int add_two(int value)
{
	return value + 2;
}

int
main(void)
{
	extern inline int add_two(int value);
	return add_two(40);
}
