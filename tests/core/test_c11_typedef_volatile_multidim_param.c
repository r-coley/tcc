typedef volatile int vmatrix2x2_t[2][2];

int
sum(vmatrix2x2_t a)
{
	return a[0][0] + a[0][1] + a[1][0];
}

int
main(void)
{
	int m[2][2] = {{40, 1}, {1, 0}};
	return sum(m);
}
