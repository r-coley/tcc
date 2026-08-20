int values[4];
int (*shared_ptr)[4];

static void
check_decl(void)
{
	extern int (*shared_ptr)[4];

	(void)shared_ptr;
}

int
main(void)
{
	check_decl();
	return 42;
}
