int values[4] = {42, 0, 0, 0};
int (*ptr)[4] = &values;

int
main(void)
{
	return (*ptr)[0];
}
