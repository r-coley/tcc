struct static_assert {
	int thread_local;
};

enum {
	nullptr = 36
};

static int
true(int false)
{
	int bool = 1;

	goto nullptr_t;
nullptr_t:
	return false + bool;
}

int
main(void)
{
	struct static_assert obj;

	obj.thread_local = 5;
	return true(obj.thread_local) + nullptr;
}
