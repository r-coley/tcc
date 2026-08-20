int *shared[];
int *shared[];
int extern *shared[];

int
set_shared_pointer_after_type(void);

int
main(void)
{
	if (shared[0] != 0)
		return 1;
	return set_shared_pointer_after_type();
}
