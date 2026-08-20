typedef int (*old_cb)();
typedef int (*proto_cb)(int *);
typedef int (*old_outer)(old_cb);
typedef int (*proto_outer)(proto_cb);

struct holder { int pad; proto_outer fp; };

int
incp(int *p)
{
	return *p + 1;
}

int
call_it(proto_cb cb)
{
	int x = 41;
	return cb(&x);
}

old_outer gp;
proto_outer gp = call_it;

int
main(void)
{
	struct holder h = { .fp = gp };
	return h.fp(incp) - 42;
}
