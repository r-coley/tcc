int extern inline add_one(int value)
{
	return value + 1;
}

int
main(void)
{
	int extern inline add_one(int value);
	return add_one(41);
}
