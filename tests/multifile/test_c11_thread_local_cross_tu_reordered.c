extern int _Thread_local tls_value;

int bump_tls_from_lib_reordered(void);

int
main(void)
{
	tls_value = 40;
	return bump_tls_from_lib_reordered();
}
