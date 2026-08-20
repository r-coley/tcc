char *
returns_char_ptr(void)
{
	return 0;
}

int
main(void)
{
	int *ip;

	ip = returns_char_ptr();
	return 0;
}
