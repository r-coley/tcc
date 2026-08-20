static int
sum_register(register int x, register int y)
{
	return x + y;
}

static int
call_with_proto(register int x);

static int
call_with_proto(register int x)
{
	return x + 1;
}

int
main(void)
{
	if (sum_register(20, 22) != 42)
		return 1;
	if (call_with_proto(41) != 42)
		return 2;
	return 42;
}
