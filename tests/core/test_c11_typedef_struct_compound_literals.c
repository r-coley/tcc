typedef struct {
	int x;
	int y;
} Pair;

typedef struct {
	Pair pair;
	int z;
} Wrap;

static int
sum_pair(Pair p)
{
	return p.x + p.y;
}

int
main(void)
{
	Pair p = (Pair){ 1, 2 };
	Wrap w = (Wrap){ { 3, 4 }, 5 };
	Pair *pp = &(Pair){ 6, 7 };
	int total = 0;

	if (p.x != 1 || p.y != 2)
		return 1;
	if (w.pair.x != 3 || w.pair.y != 4 || w.z != 5)
		return 2;
	if (pp->x != 6 || pp->y != 7)
		return 3;

	pp->y = 8;
	if (pp->y != 8)
		return 4;

	total += sum_pair((Pair){ 9, 10 });
	total += ((Pair){ 11, 12 }).x;
	total += ((Wrap){ { 13, 14 }, 15 }).pair.y;

	return total == (19 + 11 + 14) ? 42 : total;
}
