extern int _Thread_local tls_value;
extern int _Thread_local tls_zero;

int _Thread_local tls_value = 40;
static int _Thread_local tls_static = 1;
int _Thread_local tls_zero;

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
