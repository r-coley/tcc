typedef int M32[3][2];

int
main(void)
{
	int (*rowp)[2] = &((M32){ [1] = {10, 20}, [0 ... 1] = {7, 8}, [2] = {30, 40} })[1];
	int value = ((M32){ [1] = {10, 20}, [0 ... 1] = {7, 8}, [2] = {30, 40} })[2][1];

	if ((*rowp)[0] != 7 || (*rowp)[1] != 8)
		return 1;
	if (value != 40)
		return 2;

	(*rowp)[0] = 41;
	if ((*rowp)[0] != 41)
		return 3;

	return 42;
}
