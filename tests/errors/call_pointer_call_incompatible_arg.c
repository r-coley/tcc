char *
returns_char_ptr(void)
{
	return 0;
}

void
takes_int_ptr(int *ip)
{
	(void)ip;
}

int
main(void)
{
	takes_int_ptr(returns_char_ptr());
	return 0;
}
