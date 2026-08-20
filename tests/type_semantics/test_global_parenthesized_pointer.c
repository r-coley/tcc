int value = 7;
int (*ptr);

int
main(void)
{
	ptr = &value;
	return *ptr - 7;
}
