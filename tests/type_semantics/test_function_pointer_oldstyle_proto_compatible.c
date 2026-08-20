typedef int (*old_fn)();
typedef int (*proto_fn)(int);

int
id_int(int x)
{
	return x;
}

int
call_proto(proto_fn fn, int value)
{
	return fn(value);
}

int
main(void)
{
	old_fn a = id_int;
	proto_fn b = a;

	return call_proto(b, 42) - 42;
}
