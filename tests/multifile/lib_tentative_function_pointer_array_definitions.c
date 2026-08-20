static int
forty_two(void)
{
	return 42;
}

extern int (*shared[])(void);

int
set_shared_function_pointer(void)
{
	shared[0] = forty_two;
	return shared[0]();
}
