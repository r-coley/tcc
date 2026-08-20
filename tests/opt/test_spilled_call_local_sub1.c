typedef unsigned long size_t;

char *strncpy(char *dst, const char *src, size_t n);

int
main(void)
{
	char dst[64];
	char src[64];
	size_t n = 4;
	int i;

	for (i = 0; i < 64; i++) {
		dst[i] = 0;
		src[i] = 0;
	}
	src[0] = 'a';
	src[1] = 'b';
	src[2] = 'c';
	src[3] = 'd';

	strncpy(dst, src, n - 1);

	if (dst[0] != 'a' || dst[1] != 'b' || dst[2] != 'c')
		return 1;
	if (dst[3] != 0)
		return 2;
	return 42;
}
