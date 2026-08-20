#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

extern _Complex float make_complex_float_parts(float real, float imag);
extern int consume_complex_float_parts(_Complex float value);

int
main(void)
{
	_Complex float value = make_complex_float_parts(3.0f, 4.0f);
	return consume_complex_float_parts(value);
}
