enum {
	COUNTER_A = __COUNTER__,
	COUNTER_B = __COUNTER__,
	COUNTER_C = __COUNTER__
};

#define COUNTER_PLUS_ONE (__COUNTER__ + 1)

int
main(void)
{
	if (COUNTER_A != 0) return 1;
	if (COUNTER_B != 1) return 2;
	if (COUNTER_C != 2) return 3;
	if (COUNTER_PLUS_ONE != 4) return 4;
	if (__COUNTER__ != 4) return 5;
	return 0;
}
