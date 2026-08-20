typedef int (*proto_fn)(int);

int
id_int(int x)
{
	return x;
}

int
main(void)
{
	proto_fn table[] = { id_int };
	return table[0](42) - 42;
}
