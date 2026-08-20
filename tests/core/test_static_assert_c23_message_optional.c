_Static_assert(sizeof(int) == 4);

int
main(void)
{
	static_assert(sizeof(long) == 8);
	return 42;
}
