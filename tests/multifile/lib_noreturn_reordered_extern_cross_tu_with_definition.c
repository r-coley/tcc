void _Noreturn stop_here_reordered_with_definition(volatile int *flag);

void _Noreturn
stop_here_reordered_with_definition(volatile int *flag)
{
	*flag = 1;
	for (;;)
		;
}

int
answer_reordered_with_definition(void)
{
	volatile int flag = 0;

	if (flag)
		stop_here_reordered_with_definition(&flag);

	return flag ? 1 : 42;
}
