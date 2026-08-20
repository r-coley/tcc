#define TYPEOF(x) _Generic((x), \
	int: 1, \
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
	return x;
}

static int
target_var(int x, ...)
{
	return x;
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

	if (TYPEOF(a == b) != 1)
		return 1;
	if (TYPEOF(a != b) != 1)
		return 2;
	if (TYPEOF(ok == ret_old) != 1)
		return 3;
	if (TYPEOF(ok != ret_old) != 1)
		return 4;
	if (TYPEOF(va == vb) != 1)
		return 5;
	if (TYPEOF(va != vb) != 1)
		return 6;

	if ((a == b) != 1)
		return 7;
	if ((a != b) != 0)
		return 8;
	if ((ok == ret_old) != 1)
		return 9;
	if ((ok != ret_old) != 0)
		return 10;
	if ((va == vb) != 1)
		return 11;
	if ((va != vb) != 0)
		return 12;

	return 42;
}
