static char *
ret_ok(void)
{
	return "ok";
}

int
main(void)
{
	char *(*fn)(void) = ret_ok;
	char *value = fn();

	return (value &&
	        value[0] == 'o' &&
	        value[1] == 'k' &&
	        value[2] == '\0') ? 42 : 1;
}
