typedef const volatile void *cvvp_t;

int
main(void)
{
	int x = 0;
	const volatile void *p = &x;
	return _Generic(p, const volatile void *: 1, cvvp_t: 2, default: 3);
}
