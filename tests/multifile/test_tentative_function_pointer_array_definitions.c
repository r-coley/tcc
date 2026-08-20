int (*shared[])(void);
int (*shared[])(void);
extern int (*shared[])(void);

int
set_shared_function_pointer(void);

int
main(void)
{
	if (shared[0] != 0)
		return 1;
	return set_shared_function_pointer();
}
