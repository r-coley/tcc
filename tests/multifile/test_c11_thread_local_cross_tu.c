extern _Thread_local int tls_value;

int bump_tls_from_lib(void);

int
main(void)
{
	tls_value = 40;
	return bump_tls_from_lib();
}
