static int value;

static void
set_value(void)
{
	value = 42;
}

struct callback_box {
	void (**slot)(void);
};

int
main(void)
{
	struct callback_box box;
	void (*callback)(void) = set_value;
	void (**slot)(void);

	slot = &callback;
	box.slot = slot;
	box.slot[0]();
	return value;
}
