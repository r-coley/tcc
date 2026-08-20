int
main(void)
{
	int *elem = &((int[6]){ [1] = 11, [3 ... 4] = 22, [0] = 7 })[4];
	int (*arrp)[6] = &((int[6]){ [2] = 5, [5] = 9 });
	int value = ((int[5]){ [4] = 17, [1 ... 2] = 3 })[4];

	if (*elem != 22)
		return 1;
	if ((*arrp)[0] != 0 || (*arrp)[2] != 5 || (*arrp)[5] != 9)
		return 2;
	if (value != 17)
		return 3;

	(*arrp)[2] = 41;
	if ((*arrp)[2] != 41)
		return 4;

	return 42;
}
