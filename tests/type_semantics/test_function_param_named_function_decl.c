int
target(void)
{
	return 7;
}

int
call_named(int fn())
{
	return fn();
}

int
call_pointer(int (*fn)(void))
{
	return fn();
}

int
main(void)
{
	return call_named(target) + call_pointer(target) - 14;
}
