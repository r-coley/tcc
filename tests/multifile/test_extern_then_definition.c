extern int shared_value;
int shared_value = 40;

int add_two(void);

int
main(void)
{
	return add_two();
}
