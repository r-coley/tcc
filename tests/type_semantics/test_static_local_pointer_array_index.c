int
main(void)
{
	static const char *months[] = { "Jan", "Feb", "Mar", "Apr" };

	if (months[3][0] != 'A')
		return 1;
	if (months[0][1] != 'a')
		return 2;
	if (months[2][2] != 'r')
		return 3;
	return 42;
}
