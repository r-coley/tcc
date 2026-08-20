typedef int (*old_cb)();
typedef int (*proto_cb)(int *);
typedef int (*old_outer)(old_cb);
typedef int (*proto_outer)(proto_cb);

int
call_it(proto_cb cb)
{
	int x = 7;
	return cb(&x);
}

int
bad(char *p)
{
	return *p;
}

old_outer gp;
proto_outer gp = call_it;

int
main(void)
{
	proto_outer arr[2] = { [1] = gp };
	return arr[1](bad);
}
