union Number {
	int i;
	unsigned u;
};

static int
read_int(union Number n)
{
	return n.i;
}

int
main(void)
{
	union Number a = (union Number){ .i = 40 };
	union Number b = ((union Number){ .u = 2U });
	union Number *pa = &(union Number){ .i = 39 };
	union Number *pb = &((union Number){ .u = 3U });
	int total = 0;

	if (a.i != 40)
		return 1;
	if (b.u != 2U)
		return 2;
	if (pa->i != 39)
		return 3;
	if (pb->u != 3U)
		return 4;

	pa->i = 20;
	pb->u = 22U;

	if (pa->i != 20)
		return 5;
	if (pb->u != 22U)
		return 6;

	total += read_int((union Number){ .i = 10 });
	total += ((union Number){ .u = 11U }).u;
	total += ((union Number){ .i = 21 }).i;

	return total == 42 ? 42 : total;
}
