#define BAD_PRAGMA _Pragma("foo" "bar(123)")

BAD_PRAGMA

int
main(void)
{
	return 0;
}
