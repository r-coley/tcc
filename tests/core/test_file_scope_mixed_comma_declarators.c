int add_one(int x), values[2], *value_ptr;
typedef int Count;
Count counter, add_two(int x), extra[1];

int backing = 39;
int *value_ptr = &backing;
int values[2] = {1, 1};
int counter = -1;
int extra[1] = {0};

int
add_one(int x)
{
	return x + 1;
}

int
add_two(int x)
{
	return x + 1;
}

int
main(void)
{
	return add_one(*value_ptr) + values[0] + values[1] + add_two(counter) + extra[0];
}
