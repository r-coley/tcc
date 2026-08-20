struct Pair {
	int x;
	int y;
};

struct Wrap {
	struct Pair pair;
	int z;
};

static int
sum_pair(struct Pair p)
{
	return p.x + p.y;
}

int
main(void)
{
	struct Pair p = (struct Pair){1, 2};
	struct Wrap w = (struct Wrap){ {3, 4}, 5 };
	int *arr = (int[]){6, 7, 8};
	struct Pair *pp = &(struct Pair){9, 10};
	int total = 0;

	if (p.x != 1 || p.y != 2)
		return 1;
	if (w.pair.x != 3 || w.pair.y != 4 || w.z != 5)
		return 2;
	if (arr[0] != 6 || arr[1] != 7 || arr[2] != 8)
		return 3;
	if (pp->x != 9 || pp->y != 10)
		return 4;

	arr[1] = 11;
	pp->y = 12;

	if (arr[1] != 11)
		return 5;
	if (pp->y != 12)
		return 6;

	total += sum_pair((struct Pair){13, 14});
	total += ((struct Pair){15, 16}).x;
	total += ((struct Wrap){ {17, 18}, 19 }).pair.y;
	total += ((int[]){20, 21, 22})[2];

	return total == (27 + 15 + 18 + 22) ? 42 : total;
}
