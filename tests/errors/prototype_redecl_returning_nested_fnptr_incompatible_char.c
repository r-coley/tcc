typedef int (*step_fn)(int);
typedef step_fn (*chooser_fn)(int);

int
(*(*maker(void))(int))(int);

typedef int (*bad_step_fn)(char);
typedef bad_step_fn (*bad_chooser_fn)(int);

bad_chooser_fn
maker(void);

int
main(void)
{
	return 0;
}
