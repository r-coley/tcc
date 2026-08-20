typedef int (*vfn_t)(int, ...);

static int
add1(int x, ...)
{
	return x + 1;
}

int
main(void)
{
	vfn_t p = add1;
	return _Generic(p, int (* const volatile)(int, ...): 1, vfn_t: 2, default: 3);
}
