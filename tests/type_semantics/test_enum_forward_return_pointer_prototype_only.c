enum E;

enum E *return_null(void);

enum E { VALUE = 42 };

enum E *
return_null(void)
{
	return 0;
}

int
main(void)
{
	return return_null() == 0 ? 0 : 1;
}
