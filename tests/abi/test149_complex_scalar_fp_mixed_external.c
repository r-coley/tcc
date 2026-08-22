extern int check_complex_scalar_fp_mixed(double, _Complex float, double,
                                         _Complex double, float);

int
main(void)
{
	return check_complex_scalar_fp_mixed(1.5, (_Complex float)2.5f, 3.5,
	                                    (_Complex double)4.5, 5.5f);
}
