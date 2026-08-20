#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
#error designated initializers require C99 or later
#endif

typedef struct Entry {
	char name[16];
	void *pointer;
	int count;
	int size;
	int align;
	int complete;
} Entry;

static Entry *
get_entry(void)
{
	static Entry entry = {
		.name = "entry",
		.pointer = 0,
		.count = 3,
		.size = 16,
		.align = 8,
		.complete = 1
	};

	return &entry;
}

int
main(void)
{
	Entry *entry = get_entry();

	if (entry->name[0] != 'e' || entry->name[4] != 'y')
		return 1;
	if (entry->pointer != 0)
		return 2;
	if (entry->count != 3 || entry->size != 16)
		return 3;
	if (entry->align != 8 || entry->complete != 1)
		return 4;
	return 42;
}
