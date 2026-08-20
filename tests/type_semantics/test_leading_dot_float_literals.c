float global_float = .5f;
double global_double = .25;
double global_exp = .5e1;

int
main(void)
{
	float local_float = .125f;
	double local_double = .75;

	if (global_float != 0.5f)
		return 1;
	if (global_double != 0.25)
		return 2;
	if (global_exp != 5.0)
		return 3;
	if (local_float != 0.125f)
		return 4;
	if (local_double != 0.75)
		return 5;
	return 0;
}
