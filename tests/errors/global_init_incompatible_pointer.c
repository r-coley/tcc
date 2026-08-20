int global_int;
char *global_char_ptr = &global_int;

int
main(void)
{
	return global_char_ptr != 0;
}
