void _Noreturn extern stop(void);

void _Noreturn
stop(void)
{
	for (;;)
		;
}

int
main(void)
{
	return 42;
}
