static int value = 40;

int
main(void)
{
	extern int value;
	return value + 2;
}
