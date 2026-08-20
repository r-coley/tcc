#include <stddef.h>

struct et_info {
	char fmttype;
	unsigned char base;
	unsigned char flags;
	unsigned char type;
	unsigned char charset;
	unsigned char prefix;
};

static const struct et_info fmtinfo[] = {
	{ 'd', 10, 1, 16, 0, 0 },
	{ 's', 0, 4, 5, 0, 0 },
};

int main(void)
{
	int idx = 0;

	if (fmtinfo[idx].fmttype != 'd')
		return 1;
	if (fmtinfo[1].fmttype != 's')
		return 2;
	if (sizeof(fmtinfo[0]) != 6)
		return 3;
	return 42;
}
