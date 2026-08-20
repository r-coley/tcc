typedef int (*old_cb)();
typedef int (*proto_cb)(int *);
typedef int (*old_outer)(old_cb);
typedef int (*proto_outer)(proto_cb);

struct holder {
	proto_outer fp;
};

static int
idp_old(p)
int *p;
{
	return *p;
}

static int
idp_proto(int *p)
{
	return *p;
}

static int
call_it(proto_cb cb)
{
	int x = 42;
	return cb(&x);
}

old_outer gp;
proto_outer gp = call_it;

extern old_outer ext_gp;
proto_outer ext_gp = call_it;

int
main(void)
{
	proto_outer local = gp;
	proto_outer arr[2] = { [1] = gp };
	struct holder h = { gp };
	int pick = 1;
	old_outer fallback = gp;
	old_cb oldf = idp_old;
	proto_cb protof = idp_proto;

	if (gp(oldf) != 42)
		return 1;
	if (ext_gp(oldf) != 42)
		return 2;
	if (local(oldf) != 42)
		return 3;
	if (arr[1](oldf) != 42)
		return 4;
	if (h.fp(oldf) != 42)
		return 5;
	if ((pick ? gp : fallback)(oldf) != 42)
		return 6;
	if ((0 ? fallback : gp)(oldf) != 42)
		return 7;
	if (gp(protof) != 42)
		return 8;

	return 42;
}
