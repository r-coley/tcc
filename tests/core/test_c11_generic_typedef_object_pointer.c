typedef int *ip;
typedef const int *cip;
typedef void *vp;

#define MATCH_IP(x) _Generic((x), ip: 1, default: 99)
#define MATCH_DIRECT_IP(x) _Generic((x), int *: 2, default: 99)
#define MATCH_CIP(x) _Generic((x), cip: 3, default: 99)
#define MATCH_DIRECT_CIP(x) _Generic((x), const int *: 4, default: 99)
#define MATCH_VP(x) _Generic((x), vp: 5, default: 99)
#define MATCH_DIRECT_VP(x) _Generic((x), void *: 6, default: 99)

int
main(void)
{
	int value = 0;
	ip p = &value;
	cip cp = &value;
	vp pv = &value;

	if (MATCH_IP(p) != 1)
		return 1;
	if (MATCH_DIRECT_IP(p) != 2)
		return 2;
	if (MATCH_CIP(cp) != 3)
		return 3;
	if (MATCH_DIRECT_CIP(cp) != 4)
		return 4;
	if (MATCH_VP(pv) != 5)
		return 5;
	if (MATCH_DIRECT_VP(pv) != 6)
		return 6;

	return 42;
}
