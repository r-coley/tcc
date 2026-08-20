#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error complex types require C99 or later
#endif

static _Complex double
make_complex(double real, double imaginary)
{
	_Complex double value = (_Complex double)real;
	((double *)&value)[1] = imaginary;
	return value;
}

int
main(void)
{
	_Complex double complex_true = make_complex(0.0, 2.0);
	_Complex double complex_false = make_complex(0.0, 0.0);
	_Imaginary double imaginary_true = (_Imaginary double)3.0;
	_Imaginary double imaginary_false = (_Imaginary double)0.0;
	int count = 0;
	int side_effect = 0;
	int i;

	if (!complex_true || !!complex_false)
		return 1;
	if (!imaginary_true || !!imaginary_false)
		return 2;

	if (!(complex_true && 1) || complex_false && ++side_effect)
		return 3;
	if (side_effect != 0)
		return 4;
	if (!(complex_false || complex_true))
		return 5;
	if (!(imaginary_true && complex_true))
		return 6;
	if (imaginary_false || complex_false)
		return 7;

	if (complex_true)
		count++;
	if (complex_false)
		return 8;
	if (imaginary_true)
		count++;
	if (imaginary_false)
		return 9;

	i = 0;
	while (complex_true && i < 1) {
		count++;
		i++;
	}

	i = 0;
	do {
		count++;
		i++;
	} while (imaginary_true && i < 1);

	for (i = 0; complex_true && i < 1; i++)
		count++;

	count += complex_true ? 1 : 100;
	count += imaginary_true ? 1 : 100;
	count += complex_false ? 100 : 1;
	count += imaginary_false ? 100 : 1;

	if (!make_complex(0.0, 4.0))
		return 10;
	if (make_complex(0.0, 0.0))
		return 11;
	if (!(make_complex(0.0, 5.0) && 1))
		return 12;
	if (!(make_complex(0.0, 6.0) ? 1 : 0))
		return 13;

	if (count != 9)
		return 14;
	return 42;
}
