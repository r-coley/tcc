#include <stdnoreturn.h>

noreturn void
stop_now(void)
{
	for (;;)
		;
}

int
main(void)
{
	void (*fp)(void) = stop_now;
	return fp == stop_now ? 42 : 1;
}
