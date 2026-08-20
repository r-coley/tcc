typedef int (*old_fn)();
typedef int (*new_fn)(int);

int
target(int x)
{
	return x;
}

old_fn ret(void);
new_fn ret(void);

old_fn
ret(void)
{
	return target;
}

int
main(void)
{
	return ret()(8) - 8;
}
