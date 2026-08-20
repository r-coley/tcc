int
main(void)
{
	const char *src = "abc";
	const unsigned char *u = src;
	const char *c = u;

	if (u[0] != 'a') return 1;
	if (c[1] != 'b') return 2;
	return 0;
}
