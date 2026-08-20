typedef int (*proto_fn)(int);

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
	return s.fn(42) - 42;
}
