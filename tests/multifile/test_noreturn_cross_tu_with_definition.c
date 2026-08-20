extern _Noreturn void stop_here_with_definition(volatile int *flag);

int answer_with_definition(void);

int
main(void)
{
	return answer_with_definition();
}
