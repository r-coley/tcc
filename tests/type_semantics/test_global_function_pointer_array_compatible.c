typedef int (*proto_fn)(int);

int
id_int(int x)
{
	return x;
}

proto_fn table[] = { id_int };

int
main(void)
{
	return table[0](42) - 42;
}
