int
check_complex_double_xmm_spill_mixed(int tag, _Complex double a, _Complex double b,
	                                _Complex double c, _Complex double d,
	                                _Complex double e)
{
	if (tag != 17 || (double)a != 1.5 || (double)b != 2.5 ||
	    (double)c != 3.5 || (double)d != 4.5 || (double)e != 5.5)
		return 1;
	return 42;
}
