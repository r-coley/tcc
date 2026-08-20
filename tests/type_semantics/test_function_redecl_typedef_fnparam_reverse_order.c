typedef int fn_t(void);

int
target(void)
{
	return 7;
}

int call(int (*fn)(void));
int call(fn_t fn);

int
call(int (*fn)(void))
{
	return fn();
}

int
main(void)
{
	return call(target) - 7;
}
