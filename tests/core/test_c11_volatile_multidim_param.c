int
sum(volatile int a[2][2])
{
	return a[0][0] + a[0][1] + a[1][0];
}

int
main(void)
{
	int m[2][2] = {{40, 1}, {1, 0}};
	return sum(m);
}
