#define TYPEOF(x) _Generic((x), \
	enum Small: 1, \
	int: 2, \
	unsigned int: 3, \
	default: 99)

enum Small {
	EN_NEG = -1,
	EN_BIG = 300
};

static enum Small
ret_enum(unsigned int x)
{
	return x;
}

static unsigned int
take_uint(enum Small e)
{
	return e;
}

int
main(void)
{
	enum Small e_from_ui = 300U;
	enum Small e_from_neg = -1;
	unsigned int ui_from_enum = EN_NEG;

	if (TYPEOF(e_from_ui) != 1)
		return 1;
	if (e_from_ui != EN_BIG)
		return 2;
	if (TYPEOF(e_from_neg) != 1)
		return 3;
	if (e_from_neg != EN_NEG)
		return 4;
	if (TYPEOF(ui_from_enum) != 3)
		return 5;
	if (ui_from_enum != (unsigned int)-1)
		return 6;
	if (ret_enum(300U) != EN_BIG)
		return 7;
	if (ret_enum((unsigned int)-1) != EN_NEG)
		return 8;
	if (take_uint(EN_NEG) != (unsigned int)-1)
		return 9;

	return 42;
}
