typedef int (*old_cb)();
typedef int (*proto_cb)(int *);

typedef int (*old_outer)(old_cb);
typedef int (*proto_outer)(proto_cb);

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

extern old_outer gp;
proto_outer gp = call_it;

int
main(void)
{
	return gp(incp) - 42;
}
