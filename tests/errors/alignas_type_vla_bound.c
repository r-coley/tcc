int n = 4;

_Alignas(int[n]) int value;

int
main(void)
{
	return value;
}
