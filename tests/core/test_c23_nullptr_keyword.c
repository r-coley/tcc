int *
global_ptr = nullptr;

int
main(void)
{
	int *local_ptr = nullptr;
	if (global_ptr != 0)
		return 1;
	if (local_ptr != 0)
		return 1;
	return 42;
}
