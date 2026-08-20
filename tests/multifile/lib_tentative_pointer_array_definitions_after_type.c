static int value = 42;

int extern *shared[];

int
set_shared_pointer_after_type(void)
{
	shared[0] = &value;
	return *shared[0];
}
