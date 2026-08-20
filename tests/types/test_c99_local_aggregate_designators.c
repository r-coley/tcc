struct Inner {
	int a;
	int b;
};

struct Outer {
	int head;
	struct Inner inner;
	int tail[3];
};

int
main(void)
{
	struct Outer value = { .inner.b = 7, .tail = { [1] = 30 }, .head = 5 };
	return value.head + value.inner.a + value.inner.b +
	       value.tail[0] + value.tail[1] + value.tail[2];
}
