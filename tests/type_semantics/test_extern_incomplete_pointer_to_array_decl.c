extern int (*pa)[];
int (*pa)[2];
int values[2];

int
main(void)
{
	pa = &values;
	(*pa)[0] = 40;
	(*pa)[1] = 2;
	return (*pa)[0] + (*pa)[1];
}
