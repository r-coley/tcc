extern int (*pa)[4];
int values[4];
int (*pa)[4];

int
main(void)
{
	pa = &values;
	(*pa)[0] = 7;
	return (*pa)[0] - 7;
}
