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
	int (* const cp_old)() = oldstyle;
	int (* volatile vp_proto)(int) = proto;
	int (* const volatile cvp_old)() = oldstyle;
	int (** const cpp_old)() = &p_old;
	int (** volatile vpp_proto)(int) = &p_proto;
	int (** const volatile cvpp_old)() = &p_old;
	int total = 0;

	total += _Generic(cp_old, int (*)(int): 10, default: 100);
	total += _Generic(vp_proto, int (*)(): 20, default: 100);
	total += _Generic(cvp_old, int (*)(int): 30, default: 100);
	total += _Generic(cpp_old, int (**)(int): 40, default: 100);
	total += _Generic(vpp_proto, int (**)(): 50, default: 100);
	total += _Generic(cvpp_old, int (**)(int): 60, default: 100);

	return total == 210 ? 42 : total;
}
