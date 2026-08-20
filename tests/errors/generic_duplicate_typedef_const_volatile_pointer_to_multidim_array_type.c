typedef const volatile int cvmatrix_t[2][2];

int
main(void)
{
	const volatile int matrix[2][2] = {{0}};
	return _Generic(&matrix, const volatile int (*)[2][2]: 1, cvmatrix_t *: 2, default: 3);
}
