int thread_local tls_value;

int
bump_tls_from_lib_reordered(void)
{
	tls_value += 2;
	return tls_value;
}
