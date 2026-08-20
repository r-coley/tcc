float global_trailing_dot = 1.;
float global_trailing_dot_suffix = 2.f;
double global_decimal_exp = 3E1;
float global_decimal_exp_signed = .5e+1f;
double global_hex_leading_frac = 0x.8p2;
float global_hex_upper = 0X1P-1F;

int
main(void)
{
	double local_trailing_dot_exp = 4.e-1;
	float local_hex_frac = 0x1.8p+1f;

	if (global_trailing_dot != 1.0f)
		return 1;
	if (global_trailing_dot_suffix != 2.0f)
		return 2;
	if (global_decimal_exp != 30.0)
		return 3;
	if (global_decimal_exp_signed != 5.0f)
		return 4;
	if (global_hex_leading_frac != 2.0)
		return 5;
	if (global_hex_upper != 0.5f)
		return 6;
	if (local_trailing_dot_exp != 0.4)
		return 7;
	if (local_hex_frac != 3.0f)
		return 8;
	return 0;
}
