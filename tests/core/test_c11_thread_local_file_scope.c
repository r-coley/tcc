extern _Thread_local int tls_value;

_Thread_local int tls_value = 40;
static _Thread_local int tls_static = 1;
_Thread_local int tls_zero;

int
main(void)
{
	tls_zero = 1;
	return tls_value + tls_static + tls_zero;
}
