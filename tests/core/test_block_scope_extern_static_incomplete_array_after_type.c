int static values[];
int static values[3] = { 4, 5, 33 };

int
main(void)
{
	int extern values[];
	return values[0] + values[1] + values[2];
}
