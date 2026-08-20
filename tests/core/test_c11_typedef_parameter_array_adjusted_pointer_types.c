#define PARAM_EXPR_TYPE(x) _Generic((x), \
	int *: 1, \
	const volatile int *: 2, \
	default: 99)

#define PARAM_ADDR_TYPE(x) _Generic(&(x), \
	int **: 1, \
	const volatile int **: 2, \
	default: 99)

typedef int a3_t[3];
typedef const volatile int cva3_t[3];

static int
check_plain(a3_t a)
{
	if (PARAM_EXPR_TYPE(a) != 1)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 1)
		return 2;
	return 0;
}

static int
check_const_volatile(cva3_t a)
{
	if (PARAM_EXPR_TYPE(a) != 2)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 2)
		return 2;
	return 0;
}

int
main(void)
{
	int a[3] = {1, 2, 3};
	const volatile int cva[3] = {10, 11, 12};

	if (check_plain(a) != 0)
		return 10 + check_plain(a);
	if (check_const_volatile(cva) != 0)
		return 20 + check_const_volatile(cva);

	return 42;
}
