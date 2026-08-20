extern _Noreturn void stop_here_with_definition(volatile int *flag);

_Noreturn void
stop_here_with_definition(volatile int *flag)
{
	*flag = 1;
	for (;;)
		;
}

int
answer_with_definition(void)
{
	volatile int flag = 0;

	if (flag)
		stop_here_with_definition(&flag);

	return flag ? 1 : 42;
}
