struct Inner {
	int len;
	char data[];
};

struct Outer {
	int tag;
	struct Inner inner;
};

int
main(void)
{
	return 0;
}
