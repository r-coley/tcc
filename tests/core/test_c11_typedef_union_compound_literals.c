typedef union {
	int i;
	unsigned u;
} Value;

static int
read_i(Value v)
{
	return v.i;
}

int
main(void)
{
	Value v = (Value){ .i = 9 };
	Value *vp = &(Value){ .i = 13 };
	int total = 0;

	if (v.i != 9)
		return 1;
	if (vp->i != 13)
		return 2;

	vp->i = 17;
	if (vp->i != 17)
		return 3;

	total += read_i((Value){ .i = 5 });
	total += ((Value){ .i = 7 }).i;
	total += vp->i;

	return total == (5 + 7 + 17) ? 42 : total;
}
