int
main(void)
{
	int n = 5;
	char text[n] = "hi";

	if (text[0] != 'h')
		return 1;
	if (text[1] != 'i')
		return 2;
	if (text[2] != 0)
		return 3;
	if (text[4] != 0)
		return 4;

	return 0;
}
