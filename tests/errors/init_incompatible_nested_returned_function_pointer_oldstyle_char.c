typedef int (*old_inner)();
typedef int (*proto_inner)(char);
typedef old_inner (*old_outer)();
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

proto_outer bad = ret_old;

int
main(void)
{
	return 0;
}
