typedef int (*old_fn)();
typedef int (*char_fn)(char);

int
target(char x)
{
	return x;
}

old_fn ret(void);
char_fn ret(void);

old_fn
ret(void)
{
	return target;
}
