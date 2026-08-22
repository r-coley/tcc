int
main(void)
{
	char *plain = 0;
	unsigned char *unsigned_ptr = 0;

	plain = unsigned_ptr;
	return plain != 0;
}
