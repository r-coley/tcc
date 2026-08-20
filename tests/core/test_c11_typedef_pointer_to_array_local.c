typedef int row4_t[4];
typedef row4_t *row4_ptr_t;

int
main(void)
{
	row4_t a = {40, 1, 1, 0};
	row4_ptr_t p = &a;
	return (*p)[0] + (*p)[1] + (*p)[2];
}
