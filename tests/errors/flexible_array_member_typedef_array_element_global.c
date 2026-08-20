struct Inner {
	int len;
	char data[];
};

typedef struct Inner Inner;

Inner arr[2];

int
main(void)
{
	return 0;
}
