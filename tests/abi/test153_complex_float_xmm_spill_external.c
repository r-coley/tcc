extern int check_complex_float_xmm_spill(_Complex float, _Complex float,
                                         _Complex float, _Complex float,
                                         _Complex float, _Complex float,
                                         _Complex float, _Complex float,
                                         _Complex float);

int
main(void)
{
	return check_complex_float_xmm_spill((_Complex float)1.5f,
	                                    (_Complex float)2.5f,
	                                    (_Complex float)3.5f,
	                                    (_Complex float)4.5f,
	                                    (_Complex float)5.5f,
	                                    (_Complex float)6.5f,
	                                    (_Complex float)7.5f,
	                                    (_Complex float)8.5f,
	                                    (_Complex float)9.5f);
}
