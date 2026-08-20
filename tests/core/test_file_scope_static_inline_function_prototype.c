static inline int f(void);

static inline int
f(void)
{
	return 42;
}

int
main(void)
{
	return f();
}
