typedef int (*old_inner)();
typedef int (*proto_inner)(int *);

typedef old_inner (*old_outer)();
typedef proto_inner (*proto_outer)(int);

struct holder {
	proto_outer fp;
};

static int
idp(int *p)
{
	return *p;
}

static proto_inner
ret_proto(int seed)
{
	(void)seed;
	return idp;
}

old_outer maker;
proto_outer maker = ret_proto;

extern old_outer ext_maker;
proto_outer ext_maker = ret_proto;

int
main(void)
{
	proto_outer local = maker;
	proto_outer arr[2] = { [1] = maker };
	struct holder h = { maker };
	int pick = 1;
	old_outer fallback = maker;
	int v = 42;

	if (maker(0)(&v) != 42)
		return 1;
	if (ext_maker(0)(&v) != 42)
		return 2;
	if (local(0)(&v) != 42)
		return 3;
	if (arr[1](0)(&v) != 42)
		return 4;
	if (h.fp(0)(&v) != 42)
		return 5;
	if ((pick ? maker : fallback)(0)(&v) != 42)
		return 6;
	if ((0 ? fallback : maker)(0)(&v) != 42)
		return 7;

	return 42;
}
