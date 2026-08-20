typedef const long *clp;

#define MATCH_CLP(x) _Generic((x), clp: 10, default: 100)
#define MATCH_DIRECT_CLP(x) _Generic((x), const long *: 20, default: 100)

int
main(void)
{
	long value = 0;
	clp cp = &value;
	int total = 0;

	total += MATCH_CLP(cp);
	total += MATCH_DIRECT_CLP(cp);

	return total == 30 ? 42 : total;
}
