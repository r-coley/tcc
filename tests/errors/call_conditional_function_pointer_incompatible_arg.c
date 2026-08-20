typedef void (*old_fn)();
typedef void (*proto_fn)(int *);

void
takes_int_ptr(int *p)
{
	(void)p;
}

int
main(void)
{
	old_fn a = takes_int_ptr;
	proto_fn b = takes_int_ptr;
	char *cp = 0;

	(1 ? a : b)(cp);
	return 0;
}
