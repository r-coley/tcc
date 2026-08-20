int inline
add_one(void)
{
	return 42;
}

int inline add_one(void);

int
main(void)
{
	return add_one();
}
