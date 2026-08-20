int static value = 40;
int static *values[] = { &value };

int
main(void)
{
	int extern *values[];
	return *values[0] + 2;
}
