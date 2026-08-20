static int
oldstyle(x)
int x;
{
	return x + 1;
}

static int
proto(int x)
{
	return x + 2;
}

int
main(void)
{
	int (*p_old)() = oldstyle;
	int (*p_proto)(int) = proto;
	int total = 0;

	total += _Generic(p_old, int (*)(int): 10, default: 100);
	total += _Generic(p_proto, int (*)(): 20, default: 100);
	total += _Generic(oldstyle, int (*)(int): 30, default: 100);
	total += _Generic(proto, int (*)(): 40, default: 100);

	return total == 100 ? 42 : total;
}
