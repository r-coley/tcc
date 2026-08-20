typedef long double ld_t;

#define TYPEOF(x) _Generic((x), long double: 10, double: 20, default: 99)
#define MATCH_TYPEDEF_LD(x) _Generic((x), ld_t: 30, default: 99)
#define MATCH_DIRECT_LD(x) _Generic((x), long double: 40, default: 99)

long double add(long double a, long double b)
{
	return a + b;
}

int
main(void)
{
	long double x = add((long double)20.0, (long double)22.0);
	ld_t y = (ld_t)7.0;

	if (TYPEOF(x) != 10)
		return 1;
	if (TYPEOF(x + (long double)0.0) != 10)
		return 2;
	if (MATCH_TYPEDEF_LD(y) != 30)
		return 3;
	if (MATCH_DIRECT_LD(y) != 40)
		return 4;
	return 42;
}
