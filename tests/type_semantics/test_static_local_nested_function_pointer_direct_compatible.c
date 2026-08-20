int id(int x)
{
	return x;
}

int (*retfn(int x))(int)
{
	(void)x;
	return id;
}

int main(void)
{
	static int (*(*maker)(int))(int) = retfn;
	return maker(42)(0);
}
