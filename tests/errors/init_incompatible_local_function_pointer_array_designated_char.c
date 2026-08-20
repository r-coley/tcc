typedef int (*proto_fn)(char);

int
id_int(int x)
{
	return x;
}

int
main(void)
{
	proto_fn table[1] = { [0] = id_int };
	return 0;
}
