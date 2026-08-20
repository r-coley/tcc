_Thread_local int tls_value;

int
bump_tls_from_lib_storage_reordered(void)
{
	tls_value += 2;
	return tls_value;
}
