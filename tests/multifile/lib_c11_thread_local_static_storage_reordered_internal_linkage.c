_Thread_local static int tls_value = 20;

int
other_tls_value_storage_reordered(void)
{
	return tls_value;
}
