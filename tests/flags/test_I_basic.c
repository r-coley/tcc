/*
 * test_I_basic.c — verifies that -I adds the directory to the include search path.
 *
 * The header "answer.h" lives in tests/flags/include/ and is NOT on the
 * default search path, so this test only compiles successfully when the
 * test runner passes:  -I tests/flags/include
 *
 * Expected exit code: 42
 */
#include "answer.h"

int
main(void)
{
	return THE_ANSWER;
}
