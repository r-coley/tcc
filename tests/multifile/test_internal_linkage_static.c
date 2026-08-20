static int value = 10;

int other_value(void);

static int
self_value(void)
{
	return value;
}

int
main(void)
{
	return self_value() + other_value();
}
