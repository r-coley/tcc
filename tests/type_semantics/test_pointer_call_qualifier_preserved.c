const void *
returns_const_void_ptr(void)
{
	static int value;
	return &value;
}

int
main(void)
{
	const int *ip = returns_const_void_ptr();

	return *ip != 0;
}
