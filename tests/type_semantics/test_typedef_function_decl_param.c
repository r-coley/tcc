typedef int fn_t(void);

int
target(void)
{
	return 7;
}

int
call(fn_t fn);

int
call(int (*fn)(void));

int
call(fn_t fn)
{
	return fn();
}

int
main(void)
{
	return call(target) - 7;
}
