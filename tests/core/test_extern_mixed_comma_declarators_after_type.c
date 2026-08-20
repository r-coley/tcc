int extern ext_counter, ext_add(int x), ext_values[2], *ext_ptr;

int ext_counter = 39;
int *ext_ptr = &ext_counter;
int ext_values[2] = {1, 1};

int
ext_add(int x)
{
	return x + 1;
}

int
main(void)
{
	return ext_add(*ext_ptr) + ext_values[0] + ext_values[1];
}
