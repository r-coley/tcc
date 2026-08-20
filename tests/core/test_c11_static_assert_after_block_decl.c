int
main(void)
{
	int x = 1;
	_Static_assert(sizeof(int) == 4, "int size");
	int y = 41;
	return x + y;
}
