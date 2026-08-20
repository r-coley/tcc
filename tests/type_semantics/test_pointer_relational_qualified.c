int
main(void)
{
	int values[2];
	int *p = &values[0];
	const int *q = &values[1];

	if (!(p < q))
		return 1;
	if (!(q > p))
		return 2;
	if (!(p <= q))
		return 3;
	if (!(q >= p))
		return 4;

	return 0;
}
