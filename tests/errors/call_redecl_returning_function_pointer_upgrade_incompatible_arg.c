typedef int (*proto_fn)(int *);
typedef int (*old_fn)();

proto_fn maker(void);
old_fn maker();

int
main(void)
{
	char c = 0;
	return maker()(&c);
}
