typedef void (*old_fn)();
typedef void (*proto_fn)(int *);

old_fn gp;
proto_fn gp;

int
main(void)
{
	char c = 0;
	return gp(&c);
}
