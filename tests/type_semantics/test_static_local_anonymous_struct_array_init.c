static int g1 = 7;
static int g2 = 41;

int main(void)
{
	static const struct {
		const char *name;
		const int *ptr;
	} a[] = {
		{ "json_each", &g1 },
		{ "json_tree", &g2 },
	};

	if (a[0].name == 0)
		return 1;
	if (a[1].name == 0)
		return 2;
	if (*a[1].ptr != 41)
		return 3;
	return 42;
}
