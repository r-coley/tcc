int
main(void)
{
	const int x = 1;
	const int *p = &x;
	++*p;
	return 0;
}
