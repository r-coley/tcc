_Thread_local extern int tls_value;
_Thread_local extern int tls_zero;

_Thread_local int tls_value = 40;
_Thread_local static int tls_static = 1;
_Thread_local int tls_zero;

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
