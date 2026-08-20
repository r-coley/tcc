int static value = 40;
int static *values[1] = { &value };
int extern *values[1];

int
main(void)
{
	return *values[0] + 2;
}
