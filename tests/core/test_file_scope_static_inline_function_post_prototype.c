static inline int
f(void)
{
	return 42;
}

static inline int f(void);

int
main(void)
{
	return f();
}
