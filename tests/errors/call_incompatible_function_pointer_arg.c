typedef int (*int_fn)(void);

int
takes_int_fn(int_fn fn)
{
	return fn();
}

char *
returns_string(void)
{
	return "x";
}

int
main(void)
{
	return takes_int_fn(returns_string);
}
