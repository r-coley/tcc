typedef const void *cvp_t;
typedef volatile void *vvp_t;
typedef const volatile void *cvvp_t;

#define MATCH_CVP(x) _Generic((x), cvp_t: 10, default: 100)
#define MATCH_DIRECT_CVP(x) _Generic((x), const void *: 20, default: 100)
#define MATCH_VVP(x) _Generic((x), vvp_t: 30, default: 100)
#define MATCH_DIRECT_VVP(x) _Generic((x), volatile void *: 40, default: 100)
#define MATCH_CVVP(x) _Generic((x), cvvp_t: 50, default: 100)
#define MATCH_DIRECT_CVVP(x) _Generic((x), const volatile void *: 60, default: 100)

int
main(void)
{
	int x = 0;
	cvp_t cp = &x;
	vvp_t vp = &x;
	cvvp_t cvp = &x;
	int total = 0;

	total += MATCH_CVP(cp);
	total += MATCH_DIRECT_CVP(cp);
	total += MATCH_VVP(vp);
	total += MATCH_DIRECT_VVP(vp);
	total += MATCH_CVVP(cvp);
	total += MATCH_DIRECT_CVVP(cvp);

	return total == 210 ? 42 : total;
}
