typedef int (*int_fn)(void);

char *
returns_string(void)
{
	return "x";
}

int_fn
choose_fn(void)
{
	return returns_string;
}
