typedef void (*old_fn)();
typedef void (*proto_fn)(int *);

extern old_fn gp;
extern proto_fn gp;

int
main(void)
{
	char c = 0;
	return gp(&c);
}
