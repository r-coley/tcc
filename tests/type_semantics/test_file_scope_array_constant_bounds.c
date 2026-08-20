enum { ENUM_BOUND = 4 };

int enum_values[ENUM_BOUND];
int sizeof_values[sizeof(int)];

int
main(void)
{
	if (sizeof(enum_values) != ENUM_BOUND * sizeof(int))
		return 1;
	if (sizeof(sizeof_values) != sizeof(int) * sizeof(int))
		return 2;
	return 0;
}
