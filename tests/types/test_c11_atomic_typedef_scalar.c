typedef int myint;

int
main(void)
{
	_Atomic(myint) value = 5;
	_Atomic(myint) other = 7;

	return (value + other) == 12 ? 42 : 0;
}
