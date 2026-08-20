int
main(void)
{
	if (0)
		return 1;
	else
		thread_local int x = 1;
	return 42;
}
