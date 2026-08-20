typedef int *restrict rip_t;

int
main(void)
{
	int x = 0;
	rip_t p = &x;

	return _Generic(p, rip_t: 42, default: 1);
}
