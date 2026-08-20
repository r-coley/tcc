typedef int matrix2x2_t[2][2];

static int
sum(matrix2x2_t *p)
{
	return (*p)[0][0] + (*p)[0][1] + (*p)[1][0];
}

int
main(void)
{
	matrix2x2_t m = {{40, 1}, {1, 0}};
	return sum(&m);
}
