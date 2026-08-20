void _Noreturn extern stop_here_reordered_with_definition(volatile int *flag);

int answer_reordered_with_definition(void);

int
main(void)
{
	return answer_reordered_with_definition();
}
