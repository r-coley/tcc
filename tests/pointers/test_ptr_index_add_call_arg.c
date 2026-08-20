#include <string.h>

static int
match_next(const char **lines, int i)
{
	return strcmp(lines[i + 1], "beta") == 0;
}

int
main(void)
{
	const char *alpha = "alpha";
	const char *beta = "beta";
	const char *gamma = "gamma";
	const char *lines[3];

	lines[0] = alpha;
	lines[1] = beta;
	lines[2] = gamma;

	return match_next(lines, 0) ? 42 : 0;
}
