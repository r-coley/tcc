static int values[];
static int values[3] = { 4, 5, 33 };

int
main(void)
{
	extern int values[];
	return values[0] + values[1] + values[2];
}
