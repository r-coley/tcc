enum E;

enum E
return_value(void)
{
	return 42;
}

enum E { VALUE = 42 };

int
main(void)
{
	return return_value() == VALUE ? 0 : 1;
}
