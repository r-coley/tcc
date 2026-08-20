int (*shared[])(void);
int (*shared[])(void);
int extern (*shared[])(void);

int
set_shared_function_pointer_after_type(void);

int
main(void)
{
	if (shared[0] != 0)
		return 1;
	return set_shared_function_pointer_after_type();
}
