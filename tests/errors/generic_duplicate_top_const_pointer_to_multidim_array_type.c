typedef int matrix_t[2][3];

int
main(void)
{
	int matrix[2][3] = {{0}};
	return _Generic(&matrix, int (* const)[2][3]: 1, matrix_t *: 2, default: 3);
}
