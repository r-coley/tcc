int main(void)
{
	int *p = (int[]){1, 2, 39};
	int *q = (int[4]){3, 4};
	char *s = (char[]){'A', 'B', 0};

	if (p[0] + p[1] + p[2] != 42)
		return 1;
	if (q[0] + q[1] + q[2] + q[3] != 7)
		return 2;
	if (s[0] != 'A' || s[1] != 'B' || s[2] != 0)
		return 3;

	p[1] = 20;
	if (p[0] + p[1] + p[2] != 60)
		return 4;

	return 42;
}
