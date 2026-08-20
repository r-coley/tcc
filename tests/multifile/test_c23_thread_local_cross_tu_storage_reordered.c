thread_local extern int tls_value;

int bump_tls_from_lib_storage_reordered(void);

int
main(void)
{
	tls_value = 40;
	return bump_tls_from_lib_storage_reordered();
}
