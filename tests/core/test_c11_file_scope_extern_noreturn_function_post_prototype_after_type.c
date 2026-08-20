void _Noreturn extern
stop(void)
{
	for (;;)
		;
}

void _Noreturn extern stop(void);

int
main(void)
{
	return 42;
}
