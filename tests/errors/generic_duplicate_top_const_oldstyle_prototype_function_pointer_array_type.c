static int
f(x)
int x;
{
	return x;
}

typedef int (*old_fn_t)();
typedef old_fn_t old_arr_t[2];

int
main(void)
{
	old_arr_t arr = {f, f};
	old_arr_t * const p = &arr;
	return _Generic(p, old_arr_t *: 1, int (*(*)[2])(int): 2, default: 3);
}
