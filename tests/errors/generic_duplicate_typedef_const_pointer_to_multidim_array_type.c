typedef const int cmatrix_t[2][3];

int
main(void)
{
	const int matrix[2][3] = {{0}};
	return _Generic(&matrix, const int (*)[2][3]: 1, cmatrix_t *: 2, default: 3);
}
