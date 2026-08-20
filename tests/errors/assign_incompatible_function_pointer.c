typedef int (*int_fn)(void);
typedef char *(*charp_fn)(void);

char *
returns_string(void)
{
	return "x";
}

int
main(void)
{
	int_fn fn;
	charp_fn other = returns_string;
	fn = other;
	return 0;
}
