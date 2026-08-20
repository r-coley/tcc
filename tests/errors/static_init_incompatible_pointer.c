int global_int;

int
main(void)
{
	static char *local_char_ptr = &global_int;
	return local_char_ptr != 0;
}
