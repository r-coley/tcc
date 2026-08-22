int
check_complex_scalar_fp_mixed(double first, _Complex float packed,
                              double middle, _Complex double pair, float last)
{
	if (first != 1.5 || (float)packed != 2.5f || middle != 3.5 ||
	    (double)pair != 4.5 || last != 5.5f)
		return 1;
	return 42;
}
