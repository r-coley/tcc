#include <stdnoreturn.h>

noreturn void stop(void);

noreturn void stop(void)
{
	for (;;)
		;
}

int main(void)
{
	return 42;
}
