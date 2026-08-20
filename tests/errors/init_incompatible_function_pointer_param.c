/*
 * Prototype-sensitive function-pointer compatibility:
 * the return type matches, but the parameter list does not.
 */
typedef int (*takes_int_fn)(int);

int
returns_int_from_ptr(int *x)
{
	return x != 0;
}

int
main(void)
{
	takes_int_fn fn = returns_int_from_ptr;
	(void)fn;
	return 0;
}
