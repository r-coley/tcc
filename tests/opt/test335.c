typedef unsigned long size_t;

void *memset(void *ptr, int value, size_t count);

static int
zero_grown_tail(char **names, int old_cap, int new_cap)
{
	char *bytes = (char *)names;
	size_t start = sizeof(char *) * (size_t)old_cap;
	size_t count = sizeof(char *) * (size_t)(new_cap - old_cap);

	memset(bytes + start, 0, count);
	return 0;
}

int
main(void)
{
	char *names[16];
	int i;

	for (i = 0; i < 16; i++)
		names[i] = (char *)1;

	zero_grown_tail(names, 8, 16);

	if (names[0] != (char *)1)
		return 1;
	if (names[7] != (char *)1)
		return 2;
	if (names[8] != (char *)0)
		return 3;
	if (names[15] != (char *)0)
		return 4;

	return 42;
}
