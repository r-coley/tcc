static int st_counter, st_add(int x), st_values[2], *st_ptr;

static int st_counter = 39;
static int *st_ptr = &st_counter;
static int st_values[2] = {1, 1};

static int
st_add(int x)
{
	return x + 1;
}

int
main(void)
{
	return st_add(*st_ptr) + st_values[0] + st_values[1];
}
