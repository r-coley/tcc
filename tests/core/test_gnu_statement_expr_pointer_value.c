#include <stddef.h>

int
main(void)
{
	int x = 1;
	int y = 2;
	int *selected = ({ int *p = &x; p; });
	int *old = ({ int *p = selected; selected = &y; p; });

	if (selected != &y)
		return 1;
	if (old != &x)
		return 2;
	if ((({ int *p = old; p; })) != &x)
		return 3;
	return 42;
}
