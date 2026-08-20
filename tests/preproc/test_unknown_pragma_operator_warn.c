#define BAD_PRAGMA _Pragma("foobar(123)")

BAD_PRAGMA

int
main(void)
{
	return 0;
}
