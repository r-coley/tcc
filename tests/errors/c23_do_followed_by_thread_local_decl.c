int
main(void)
{
	do
		thread_local int x = 1;
	while (0);
	return 0;
}
