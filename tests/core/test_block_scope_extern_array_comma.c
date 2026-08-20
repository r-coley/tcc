int values[3] = { 4, 5, 6 };
int extra = 7;

int
main(void)
{
	extern int values[], extra;
	return values[1] + extra == 12 ? 42 : 1;
}
