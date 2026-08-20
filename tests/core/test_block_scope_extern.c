int shared_value = 42;

int
main(void)
{
	extern int shared_value;
	return shared_value;
}
