thread_local extern int tls_value;
thread_local extern int tls_zero;

thread_local int tls_value = 40;
thread_local static int tls_static = 1;
thread_local int tls_zero;

static int
read_tls(void)
{
	return tls_value + tls_static + tls_zero;
}

int
main(void)
{
	tls_zero = 1;
	return read_tls();
}
