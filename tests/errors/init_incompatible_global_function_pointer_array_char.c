typedef int (*proto_fn)(char);

int
id_int(int x)
{
	return x;
}

proto_fn table[] = { id_int };

int
main(void)
{
	return 0;
}
