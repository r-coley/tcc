int file_scope_value;

static_assert(sizeof(int) == 4, "int size");

int
main(void)
{
	return file_scope_value == 0 ? 42 : 1;
}
