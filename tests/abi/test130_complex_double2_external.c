#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

extern _Complex double make_complex_double(double real);
extern int consume_complex_double(_Complex double value);

int
main(void)
{
	_Complex double value = make_complex_double(24.0);
	return consume_complex_double(value);
}
