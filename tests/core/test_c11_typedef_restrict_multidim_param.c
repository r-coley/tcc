typedef int matrix2x2_t[2][2];

int
sum(matrix2x2_t restrict a)
{
	return a[0][0] + a[0][1] + a[1][0];
}

int
main(void)
{
	matrix2x2_t m = {{40, 1}, {1, 0}};
	return sum(m);
}
