char *
returns_char_ptr(void)
{
	return 0;
}

int *
returns_int_ptr(void)
{
	return returns_char_ptr();
}

int
main(void)
{
	return returns_int_ptr() != 0;
}
