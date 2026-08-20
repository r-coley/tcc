typedef int (*good_fn)(int);
typedef int (*bad_fn)(char);
typedef good_fn *good_fn_ptr;
typedef bad_fn *bad_fn_ptr;

int
target(int x)
{
	return x;
}

int
main(void)
{
	good_fn f = target;
	good_fn_ptr a = &f;
	bad_fn_ptr b = a;

	return 0;
}
