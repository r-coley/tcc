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

typedef int (*old_fn_t)();
typedef int (*proto_fn_t)(int);
typedef old_fn_t old_arr_t[2];
typedef proto_fn_t proto_arr_t[2];

int
main(void)
{
	old_arr_t arr_old = {oldstyle, oldstyle};
	proto_arr_t arr_proto = {proto, proto};
	old_arr_t * const p_old = &arr_old;
	proto_arr_t * volatile p_proto = &arr_proto;
	old_arr_t * const volatile p_cv_old = &arr_old;
	int total = 0;

	total += _Generic(p_old, int (*(*)[2])(int): 10, default: 100);
	total += _Generic(p_proto, int (*(*)[2])(): 20, default: 100);
	total += _Generic(p_cv_old, int (*(*)[2])(int): 30, default: 100);

	return total == 60 ? 42 : total;
}
