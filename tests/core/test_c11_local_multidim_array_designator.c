int
main(void)
{
	int matrix[3][2] = {
		[1] = {10, 20},
		[0 ... 1] = {7, 8},
		[2] = {30, 40}
	};

	if (matrix[0][0] != 7 || matrix[0][1] != 8)
		return 1;
	if (matrix[1][0] != 7 || matrix[1][1] != 8)
		return 2;
	if (matrix[2][0] != 30 || matrix[2][1] != 40)
		return 3;

	return 42;
}
