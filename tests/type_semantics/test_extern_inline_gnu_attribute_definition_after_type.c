typedef struct FILE FILE;

int extern __inline __attribute__((__gnu_inline__))
inline_add_one_after_type(int value, FILE *stream)
{
	(void)stream;
	if (value)
		return value + 1;
	return 1;
}

static int __attribute__((noinline))
attr_after_return_type(int value)
{
	return value - 1;
}

extern _Bool attributed_bool_prototype_after_type(const char *path) __attribute__((availability(macos, introduced=10.5)));

int
main(void)
{
	return attr_after_return_type(inline_add_one_after_type(42, 0));
}
