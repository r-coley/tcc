typedef int (*int_fn)(void);

char *
returns_string(void)
{
	return "x";
}

int
main(void)
{
	int_fn fn = returns_string;
	return 0;
}
