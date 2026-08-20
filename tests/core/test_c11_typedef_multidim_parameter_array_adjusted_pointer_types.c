typedef int a34_t[3][4];
typedef volatile int va34_t[3][4];
typedef const volatile int cva34_t[3][4];

typedef int (*plain_row_ptr_t)[4];
typedef volatile int (*volatile_row_ptr_t)[4];
typedef const volatile int (*const_volatile_row_ptr_t)[4];
typedef plain_row_ptr_t *plain_row_ptr_ptr_t;
typedef volatile_row_ptr_t *volatile_row_ptr_ptr_t;
typedef const_volatile_row_ptr_t *const_volatile_row_ptr_ptr_t;

#define PARAM_EXPR_TYPE(x) _Generic((x), \
	plain_row_ptr_t: 1, \
	volatile_row_ptr_t: 2, \
	const_volatile_row_ptr_t: 2, \
	default: 99)

#define PARAM_ADDR_TYPE(x) _Generic(&(x), \
	plain_row_ptr_ptr_t: 1, \
	volatile_row_ptr_ptr_t: 2, \
	const_volatile_row_ptr_ptr_t: 3, \
	default: 99)

static int
check_plain(a34_t a)
{
	if (PARAM_EXPR_TYPE(a) != 1)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 1)
		return 2;
	return 0;
}

static int
check_volatile(va34_t a)
{
	if (PARAM_EXPR_TYPE(a) != 2)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 2)
		return 2;
	return 0;
}

static int
check_const_volatile(cva34_t a)
{
	if (PARAM_EXPR_TYPE(a) != 2)
		return 1;
	if (PARAM_ADDR_TYPE(a) != 3)
		return 2;
	return 0;
}

int
main(void)
{
	int a[3][4] = {{0}};
	volatile int va[3][4] = {{0}};
	const volatile int cva[3][4] = {{0}};

	if (check_plain(a) != 0)
		return 10 + check_plain(a);
	if (check_volatile(va) != 0)
		return 20 + check_volatile(va);
	if (check_const_volatile(cva) != 0)
		return 30 + check_const_volatile(cva);

	return 42;
}
