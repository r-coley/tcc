extern inline int
add_one(int value)
{
	return value + 1;
}

extern inline int add_one(int value);

int
main(void)
{
	return add_one(41);
}
