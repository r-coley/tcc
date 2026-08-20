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

int (*(*maker)(int))(int);
int (*(*maker)())() = ret_proto;

int
main(void)
{
	int pick = 1;
	int (*(*fallback)())() = maker;
	return (pick ? maker : fallback)(0)(41) - 42;
}
