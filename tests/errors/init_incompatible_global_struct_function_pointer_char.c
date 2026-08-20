typedef int (*proto_fn)(char);

struct S {
	proto_fn fn;
};

int
id_int(int x)
{
	return x;
}

struct S s = { id_int };

int
main(void)
{
	return 0;
}
