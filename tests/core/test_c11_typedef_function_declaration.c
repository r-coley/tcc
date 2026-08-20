typedef int fn_t(void);

int
target(void)
{
	return 42;
}

int
main(void)
{
	fn_t target;
	return target();
}
