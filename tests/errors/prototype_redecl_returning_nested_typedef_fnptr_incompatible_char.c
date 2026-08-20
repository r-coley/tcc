typedef int (*step_fn)(int);
typedef step_fn (*chooser_fn)(void);

chooser_fn
maker(void);

typedef int (*bad_step_fn)(char);
typedef bad_step_fn (*bad_chooser_fn)(void);

bad_chooser_fn
maker(void);

int
main(void)
{
	return 0;
}
