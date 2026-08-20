#define PARAM_EXPR_TYPE(x) _Generic((x), \
	int *: 1, \
	const int *: 2, \
	volatile int *: 3, \
	const volatile int *: 4, \
	default: 99)

#define PARAM_ADDR_TYPE(x) _Generic(&(x), \
	int **: 1, \
	const int *const *: 2, \
	volatile int *volatile *: 3, \
	const volatile int *const volatile *: 4, \
	default: 99)

static int
check_plain(int a[3])
{
	if (PARAM_EXPR_TYPE(a) != 1)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 1)
		return 2;
	return 0;
}

static int
check_const(const int a[const 3])
{
	if (PARAM_EXPR_TYPE(a) != 2)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 2)
		return 2;
	return 0;
}

static int
check_volatile(volatile int a[volatile 3])
{
	if (PARAM_EXPR_TYPE(a) != 3)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 3)
		return 2;
	return 0;
}

static int
check_const_volatile(const volatile int a[const volatile 3])
{
	if (PARAM_EXPR_TYPE(a) != 4)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 4)
		return 2;
	return 0;
}

int
main(void)
{
	int a[3] = {1, 2, 3};
	const int ca[3] = {4, 5, 6};
	volatile int va[3] = {7, 8, 9};
	const volatile int cva[3] = {10, 11, 12};

	if (check_plain(a) != 0)
		return 10 + check_plain(a);
	if (check_const(ca) != 0)
		return 20 + check_const(ca);
	if (check_volatile(va) != 0)
		return 30 + check_volatile(va);
	if (check_const_volatile(cva) != 0)
		return 40 + check_const_volatile(cva);

	return 42;
}
