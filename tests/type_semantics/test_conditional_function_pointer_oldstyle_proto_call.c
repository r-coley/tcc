typedef double (*old_fn)();
typedef double (*proto_fn)(double);

double
id_double(double x)
{
	return x;
}

int
main(void)
{
	old_fn a = id_double;
	proto_fn b = id_double;

	return (1 ? a : b)(42) == 42.0 ? 0 : 1;
}
