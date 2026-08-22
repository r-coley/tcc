extern int check_complex_double_xmm_spill(_Complex double, _Complex double,
                                          _Complex double, _Complex double,
                                          _Complex double);

int
main(void)
{
	return check_complex_double_xmm_spill((_Complex double)1.5,
	                                     (_Complex double)2.5,
	                                     (_Complex double)3.5,
	                                     (_Complex double)4.5,
	                                     (_Complex double)5.5);
}
