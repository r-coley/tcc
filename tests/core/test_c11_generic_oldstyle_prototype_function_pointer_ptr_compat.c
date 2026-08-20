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
	int (**pp_old)() = &p_old;
	int (**pp_proto)(int) = &p_proto;
	int total = 0;

	total += _Generic(pp_old, int (**)(int): 10, default: 100);
	total += _Generic(pp_proto, int (**)(): 20, default: 100);

	return total == 30 ? 42 : total;
}
