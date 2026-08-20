typedef int (*old_fn)();
typedef int (*proto_fn)(char);

int
id_char(char x)
{
	return x;
}

int
main(void)
{
	old_fn a = id_char;
	proto_fn b = a;
	return b(1);
}
