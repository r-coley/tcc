#define TYPEOF(x) _Generic((x), \
	new_fn_ptr: 1, \
	proto_outer: 2, \
	variadic_outer: 3, \
	default: 99)

typedef int (*old_fn)();
typedef int (*new_fn)(int);
typedef old_fn *old_fn_ptr;
typedef new_fn *new_fn_ptr;

typedef int (*old_inner)();
typedef int (*proto_inner)(int);
typedef old_inner (*old_outer)(void);
typedef proto_inner (*proto_outer)(void);

typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

static int
target(int x)
{
	return x + 2;
}

static int
target_var(int x, ...)
{
	return x + 2;
}

static old_inner
ret_old(void)
{
	return target;
}

static variadic_inner
make_target(void)
{
	return target_var;
}

int
main(void)
{
	new_fn f = target;
	old_fn_ptr a = &f;
	new_fn_ptr b = a;
	proto_outer ok = ret_old;
	variadic_outer va = make_target;
	variadic_outer vb = make_target;

	if (TYPEOF(1 ? a : b) != 1)
		return 1;
	if ((**(1 ? a : b))(40) != 42)
		return 2;
	if (TYPEOF(0 ? a : b) != 1)
		return 3;
	if ((**(0 ? a : b))(40) != 42)
		return 4;
	if (TYPEOF(1 ? a : 0) != 1)
		return 5;
	if ((**(1 ? a : 0))(40) != 42)
		return 6;
	if (TYPEOF(0 ? 0 : b) != 1)
		return 7;
	if ((**(0 ? 0 : b))(40) != 42)
		return 8;

	if (TYPEOF(1 ? ok : ret_old) != 2)
		return 9;
	if ((1 ? ok : ret_old)()(40) != 42)
		return 10;
	if (TYPEOF(0 ? ok : ret_old) != 2)
		return 11;
	if ((0 ? ok : ret_old)()(40) != 42)
		return 12;
	if (TYPEOF(1 ? ok : 0) != 2)
		return 13;
	if ((1 ? ok : 0)()(40) != 42)
		return 14;
	if (TYPEOF(0 ? 0 : ret_old) != 2)
		return 15;
	if ((0 ? 0 : ret_old)()(40) != 42)
		return 16;

	if (TYPEOF(1 ? va : vb) != 3)
		return 17;
	if ((1 ? va : vb)()(40, 7) != 42)
		return 18;
	if (TYPEOF(0 ? va : vb) != 3)
		return 19;
	if ((0 ? va : vb)()(40, 7) != 42)
		return 20;
	if (TYPEOF(1 ? va : 0) != 3)
		return 21;
	if ((1 ? va : 0)()(40, 7) != 42)
		return 22;
	if (TYPEOF(0 ? 0 : vb) != 3)
		return 23;
	if ((0 ? 0 : vb)()(40, 7) != 42)
		return 24;

	return 42;
}
