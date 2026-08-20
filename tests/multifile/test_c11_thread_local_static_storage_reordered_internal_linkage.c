_Thread_local static int tls_value = 20;

int other_tls_value_storage_reordered(void);

static int
self_tls_value(void)
{
	return tls_value;
}

int
main(void)
{
	return self_tls_value() + other_tls_value_storage_reordered() + 2;
}
