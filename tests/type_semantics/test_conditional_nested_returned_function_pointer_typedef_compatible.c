typedef int (*old_inner)();
typedef int (*proto_inner)(int);
typedef old_inner (*old_outer)(void);
typedef proto_inner (*proto_outer)(void);

int
id1(int x)
{
	return x + 1;
}

int
id2(int x)
{
	return x + 2;
}

old_inner
ret1(void)
{
	return id1;
}

proto_inner
ret2(void)
{
	return id2;
}

int
main(void)
{
	proto_outer fn = 0 ? ret1 : ret2;

	return fn()(40) - 42;
}
