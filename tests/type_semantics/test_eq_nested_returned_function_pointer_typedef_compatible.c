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
main(void)
{
	proto_outer ok = ret_old;

	return (ok == ret_old) ? 0 : 1;
}
