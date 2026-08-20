#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error _Complex requires C99 or later
#endif

extern int consume_complex_float_mix(int left, _Complex float value, int right);
extern int consume_complex_double_mix(long left, _Complex double value, long right);

int
main(void)
{
	if (consume_complex_float_mix(7, (_Complex float)19.0f, 11) != 42)
		return 1;
	if (consume_complex_double_mix(5, (_Complex double)24.0, 13) != 42)
		return 2;
	return 42;
}
