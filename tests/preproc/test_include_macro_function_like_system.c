#define SYS_HDR() <stdbool.h>
#define WRAP() SYS_HDR()

#include WRAP()

int
main(void)
{
	bool ok = true;
	return ok ? 0 : 1;
}
