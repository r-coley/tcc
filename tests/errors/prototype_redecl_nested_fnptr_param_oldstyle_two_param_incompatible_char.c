typedef int (*old_fn)();
typedef old_fn *old_ptr;

int
call(old_ptr p);

typedef int (*bad_fn)(char, char);
typedef bad_fn *bad_ptr;

int
call(bad_ptr p);

int
main(void)
{
	return 0;
}
