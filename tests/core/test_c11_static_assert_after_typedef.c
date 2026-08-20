typedef int myint;
_Static_assert(sizeof(myint) == sizeof(int), "typedef size");

int
main(void)
{
	typedef long mylong;
	_Static_assert(sizeof(mylong) == sizeof(long), "block typedef size");
	return 42;
}
