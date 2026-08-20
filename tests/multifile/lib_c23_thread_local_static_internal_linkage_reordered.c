static int thread_local tls_value = 20;

int
other_tls_value_reordered(void)
{
	return tls_value;
}
