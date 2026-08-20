#include "../../cc/include/math.h"

int helper(void);

int
main(void)
{
	if (HUGE_VAL <= 1.0)
		return 0;
	if (!helper())
		return 0;
	return 42;
}
