int
main(void)
{
	char *plain = 0;
	unsigned char *unsigned_ptr = 0;

	return (1 ? plain : unsigned_ptr) != 0;
}
