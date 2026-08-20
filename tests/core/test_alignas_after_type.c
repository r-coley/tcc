int _Alignas(16) global_after_type;

int
main(void)
{
	int _Alignas(16) local_after_type = 7;
	unsigned long global_addr = (unsigned long)&global_after_type;
	unsigned long local_addr = (unsigned long)&local_after_type;

	if ((global_addr & 15UL) != 0)
		return 1;
	if ((local_addr & 15UL) != 0)
		return 2;

	return 42;
}
