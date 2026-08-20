typedef volatile void *vvp_t;

int
main(void)
{
	int x = 0;
	volatile void *p = &x;
	return _Generic(p, volatile void *: 1, vvp_t: 2, default: 3);
}
