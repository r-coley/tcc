int
inc(int x)
{
	return x + 1;
}

int (*ret_proto(int seed))(int)
{
	(void)seed;
	return inc;
}

extern int (*(*maker)())();
int (*(*maker)(int))(int) = ret_proto;

int
main(void)
{
	return maker(0)(41) - 42;
}
