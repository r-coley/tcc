_Thread_local int tls_value;

int
bump_tls_from_lib(void)
{
	tls_value += 2;
	return tls_value;
}
