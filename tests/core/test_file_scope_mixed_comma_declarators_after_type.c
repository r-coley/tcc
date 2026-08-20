int plain_counter, plain_add(int x), plain_values[2], *plain_ptr;
typedef int Count;
Count plain_extra, plain_add_two(int x), plain_tail[1];

int plain_counter = 39;
int *plain_ptr = &plain_counter;
int plain_values[2] = {1, 1};
int plain_extra = -1;
int plain_tail[1] = {0};

int
plain_add(int x)
{
	return x + 1;
}

int
plain_add_two(int x)
{
	return x + 1;
}

int
main(void)
{
	return plain_add(*plain_ptr) + plain_values[0] + plain_values[1] +
	       plain_add_two(plain_extra) + plain_tail[0];
}
