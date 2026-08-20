int
main(void)
{
	return sizeof(int[2][2]) == 4 * (int)sizeof(int) ? 42 : 1;
}
