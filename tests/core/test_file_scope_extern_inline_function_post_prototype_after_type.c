int extern inline
add_one(int value)
{
	return value + 1;
}

int extern inline add_one(int value);

int
main(void)
{
	return add_one(41);
}
