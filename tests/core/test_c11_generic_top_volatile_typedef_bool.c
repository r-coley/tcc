typedef _Bool mybool;

int
main(void)
{
	volatile mybool vb = (_Bool)1;
	const volatile mybool cvb = (_Bool)0;
	int total = 0;

	total += _Generic(vb, mybool: 10, default: 100);
	total += _Generic(vb, _Bool: 20, default: 100);
	total += _Generic(cvb, mybool: 30, default: 100);
	total += _Generic(cvb, _Bool: 40, default: 100);

	return total == 100 ? 42 : total;
}
