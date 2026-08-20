int
main(void)
{
	for (;;)
		thread_local int x = 1;
	return 0;
}
