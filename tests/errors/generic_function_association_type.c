int f(void)
{
	return 0;
}

int
main(void)
{
	return _Generic(f, int (void): 1, default: 2);
}
