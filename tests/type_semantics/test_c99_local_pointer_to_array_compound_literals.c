int
main(void)
{
	int (*arrp)[3] = &((int[3]){ 7, 8, 9 });
	int (*rowp)[2] = &((int[2][2]){ { 1, 2 }, { 3, 4 } })[1];
	int *elem = &((int[]){ 10, 11, 12 })[1];

	if ((*arrp)[0] != 7 || (*arrp)[2] != 9)
		return 1;
	if ((*rowp)[0] != 3 || (*rowp)[1] != 4)
		return 2;
	if (*elem != 11)
		return 3;

	(*arrp)[1] = 20;
	(*rowp)[0] = 30;
	*elem = 40;

	if ((*arrp)[1] != 20)
		return 4;
	if ((*rowp)[0] != 30)
		return 5;
	if (*elem != 40)
		return 6;

	return 42;
}
