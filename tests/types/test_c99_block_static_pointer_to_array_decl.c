int
main(void)
{
	static int (*cached_ptr)[4];

	(void)cached_ptr;
	return 42;
}
