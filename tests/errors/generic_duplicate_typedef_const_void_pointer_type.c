typedef const void *cvp_t;

int
main(void)
{
	int x = 0;
	const void *p = &x;
	return _Generic(p, const void *: 1, cvp_t: 2, default: 3);
}
