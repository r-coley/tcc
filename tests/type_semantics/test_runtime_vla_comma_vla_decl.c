int
main(void)
{
	int n = 2;
	int left[n], right[n];
	int result;

	left[0] = 1;
	left[1] = 2;
	right[0] = 39;
	right[1] = 0;

	result = left[1] + right[0] - 41;
	return result;
}
