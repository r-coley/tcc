#define TYPEOF(x) _Generic((x), int: 1, unsigned int: 2, default: 99)

struct Bits {
	signed int s31 : 31;
	unsigned int u31 : 31;
	unsigned int u32 : 32;
};

int
main(void)
{
	struct Bits b;

	b.s31 = -1;
	b.u31 = 0x7fffffffU;
	b.u32 = 0xffffffffU;

	if (TYPEOF(+b.s31) != 1)
		return 1;
	if (TYPEOF(+b.u31) != 1)
		return 2;
	if (TYPEOF(+b.u32) != 2)
		return 3;
	if (TYPEOF(b.u31 ? b.u31 : b.s31) != 1)
		return 4;
	if (TYPEOF(b.u32 ? b.u32 : b.s31) != 2)
		return 5;
	if (TYPEOF(b.u31 << 1) != 1)
		return 6;
	if (TYPEOF(b.u32 << 1) != 2)
		return 7;

	if ((b.u31 + 1) != -2147483648)
		return 8;
	if ((b.u32 + 1) != 0U)
		return 9;
	if ((b.s31 + 1) != 0)
		return 10;
	if ((b.u31 < b.s31) != 0)
		return 11;
	if ((b.u32 < b.s31) != 0)
		return 12;
	b.u31 = 1U;
	if ((b.u31 << 1) != 2)
		return 13;
	b.u32 = 1U;
	if ((b.u32 << 31) != 0x80000000U)
		return 14;

	return 42;
}
