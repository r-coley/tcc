static int
forty_two(void)
{
	return 42;
}

int extern (*shared[])(void);

int
set_shared_function_pointer_after_type(void)
{
	shared[0] = forty_two;
	return shared[0]();
}
