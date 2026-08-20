struct Inner {
	int x;
	int y;
};

struct Outer {
	int a;
	struct Inner inner;
	int z;
};

struct Outer value = { 1, 2, 3, 36 };

int
main(void)
{
	return value.a + value.inner.x + value.inner.y + value.z;
}
