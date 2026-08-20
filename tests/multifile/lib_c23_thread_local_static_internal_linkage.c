static thread_local int tls_value = 20;

int
other_tls_value(void)
{
	return tls_value;
}
