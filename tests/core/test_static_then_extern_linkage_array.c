static int values[3] = { 10, 12, 18 };
extern int values[3];

int
main(void)
{
	return values[0] + values[1] + values[2] + 2;
}
