int static inline
f(void)
{
	return 42;
}

int static inline f(void);

int
main(void)
{
	return f();
}
