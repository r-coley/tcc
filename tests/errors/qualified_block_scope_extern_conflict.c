int g = 42;

int
main(void)
{
	extern const int g;
	return g;
}
