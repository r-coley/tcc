static int value = 42;

extern int *shared[];

int
set_shared_pointer(void)
{
	shared[0] = &value;
	return *shared[0];
}
