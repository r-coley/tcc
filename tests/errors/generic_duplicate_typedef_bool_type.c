typedef _Bool mybool;

int
main(void)
{
	return _Generic((_Bool)0, _Bool: 1, mybool: 2, default: 3);
}
