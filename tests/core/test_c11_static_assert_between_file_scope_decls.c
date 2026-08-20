int file_scope_a;

_Static_assert(sizeof(long) == 8, "long size");

int file_scope_b;

int
main(void)
{
	return (file_scope_a == 0 && file_scope_b == 0) ? 42 : 1;
}
