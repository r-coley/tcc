static int a = 19;
static int b = 23;
static int *values[2] = { &a, &b };
extern int *values[2];

int
main(void)
{
	return *values[0] + *values[1];
}
