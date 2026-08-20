extern thread_local int tls_value;

thread_local int tls_value = 40;
static thread_local int tls_static = 1;
thread_local int tls_zero;

int
main(void)
{
	tls_zero = 1;
	return tls_value + tls_static + tls_zero;
}
