enum E;

int accept_value(enum E value);

enum E { VALUE = 42 };

int
accept_value(enum E value)
{
	return (int)value;
}

int
main(void)
{
	return accept_value(VALUE) == 42 ? 0 : 1;
}
