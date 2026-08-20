/*
 * test_I_multi.c — verifies that multiple -I flags all take effect and that
 * headers from each directory are found independently.
 *
 * Requires:  -I tests/flags/include  -I tests/flags/include2
 *
 * Expected exit code: 42  (BASE_VAL + EXTRA_VAL = 40 + 2)
 */
#include "constants.h"
#include "extras.h"

int
main(void)
{
	return BASE_VAL + EXTRA_VAL;
}
