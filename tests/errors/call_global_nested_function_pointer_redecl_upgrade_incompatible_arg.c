typedef int (*old_inner)();
typedef int (*proto_inner)(int *);

typedef old_inner (*old_outer)();
typedef proto_inner (*proto_outer)(int);

int
idp(int *p)
{
	return *p;
}

proto_inner
ret_proto(int seed)
{
	(void)seed;
	return idp;
}

old_outer maker;
proto_outer maker = ret_proto;

int
main(void)
{
	char c = 1;
	return maker(0)(&c);
}
