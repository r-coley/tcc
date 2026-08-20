typedef int (*proto_fn)(char);

struct S {
	proto_fn fn;
};

int
id_int(int x)
{
	return x;
}

int
main(void)
{
	struct S s = { id_int };
	return 0;
}
