typedef _Bool mybool;

int
main(void)
{
	mybool const b = (_Bool)1;
	int total = 0;

	total += _Generic(b, mybool: 10, default: 100);
	total += _Generic(b, _Bool: 20, default: 100);

	return total == 30 ? 42 : total;
}
