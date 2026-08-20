int file_scope_value;

_Static_assert(sizeof(int) == 4, "int size");

int
main(void)
{
	return file_scope_value == 0 ? 42 : 1;
}
