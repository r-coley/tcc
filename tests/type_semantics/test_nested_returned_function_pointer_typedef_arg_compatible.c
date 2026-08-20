typedef int (*old_inner)();
typedef int (*proto_inner)(int);
typedef old_inner (*old_outer)(void);
typedef proto_inner (*proto_outer)(void);

int
id_int(int x)
{
	return x;
}

old_inner
ret_old(void)
{
	return id_int;
}

int
call(proto_outer fn)
{
	return fn()(42);
}

int
main(void)
{
	old_outer fn = ret_old;

	return call(fn) - 42;
}
