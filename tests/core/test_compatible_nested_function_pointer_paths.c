typedef int (*old_cb)();
typedef int (*proto_cb)(int);
typedef old_cb *old_cb_ptr;
typedef proto_cb *proto_cb_ptr;

typedef int (*old_inner)();
typedef int (*proto_inner)(int);
typedef old_inner (*old_outer)(void);
typedef proto_inner (*proto_outer)(void);

typedef int (*variadic_inner)(int, ...);
typedef variadic_inner (*variadic_outer)(void);

static int
inc_old(x)
int x;
{
	return x + 1;
}

static int
inc_proto(int x)
{
	return x + 2;
}

static int
inc_var(int x, ...)
{
	return x + 3;
}

static int
call_param(proto_cb_ptr cbp, int x)
{
	return (*cbp)(x);
}

static proto_inner
make_proto(void)
{
	return inc_old;
}

static variadic_inner
make_var(void)
{
	return inc_var;
}

int
main(void)
{
	proto_cb direct = inc_proto;
	old_cb old_local = inc_old;
	old_cb_ptr oldp = &old_local;
	proto_cb_ptr protop = oldp;
	proto_outer outer = make_proto;
	variadic_outer var_outer = make_var;

	if (call_param(protop, 40) != 41)
		return 1;
	if (call_param(oldp, 40) != 41)
		return 2;
	if ((*protop)(40) != 41)
		return 3;
	if ((*oldp)(40) != 41)
		return 4;
	if (direct(40) != 42)
		return 5;

	if (outer()(40) != 41)
		return 6;
	if (make_proto()(40) != 41)
		return 7;

	if (var_outer()(39, 7) != 42)
		return 8;
	if (make_var()(39, 7) != 42)
		return 9;

	return 42;
}
