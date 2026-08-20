int
call(int (*(*pp))(int));

typedef int (*bad_step_fn)(char);
typedef bad_step_fn *bad_chooser_ptr;

int
call(bad_chooser_ptr pp);

int
main(void)
{
	return 0;
}
