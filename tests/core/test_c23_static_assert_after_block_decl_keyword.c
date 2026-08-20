int
main(void)
{
	int x = 1;
	static_assert(sizeof(int) == 4, "int size");
	int y = 41;
	return x + y;
}
