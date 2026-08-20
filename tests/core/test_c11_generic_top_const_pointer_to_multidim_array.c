int
main(void)
{
	int matrix[2][3] = {{0}};
	int (* const p)[2][3] = &matrix;
	int total = 0;

	total += _Generic(p, int (*)[2][3]: 10, default: 100);

	return total == 10 ? 42 : total;
}
