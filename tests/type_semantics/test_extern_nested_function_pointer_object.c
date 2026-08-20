int id(int x)
{
	return x;
}

int (*retfn(int x))(int)
{
	(void)x;
	return id;
}

extern int (*(*maker)(int))(int);
int (*(*maker)(int))(int) = retfn;

int
main(void)
{
	return maker(0)(42);
}
