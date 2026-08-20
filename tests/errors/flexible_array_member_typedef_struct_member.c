struct Inner {
	int len;
	char data[];
};

typedef struct Inner Inner;

struct Outer {
	int tag;
	Inner inner;
};

int
main(void)
{
	return 0;
}
