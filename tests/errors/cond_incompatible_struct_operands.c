struct A {
	int value;
};

struct B {
	int value;
};

int
main(void)
{
	struct A a;
	struct B b;
	1 ? a : b;
	return 0;
}
