typedef int (*good_fn)(int);
typedef int (*bad_fn)(char);
typedef good_fn *good_fn_ptr;
typedef bad_fn *bad_fn_ptr;

int
target_int(int x)
{
	return x;
}

int
target_char(char x)
{
	return x;
}

int
main(void)
{
	good_fn g = target_int;
	bad_fn b = target_char;

	return (1 ? &g : &b) != 0;
}
