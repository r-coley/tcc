static _Noreturn void
stop(void)
{
	for (;;)
		;
}

static _Noreturn void stop(void);

int
main(void)
{
	return 42;
}
