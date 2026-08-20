int
fi(int x)
{
	return x;
}

char
fc(char x)
{
	return x;
}

int
main(void)
{
	int (*ip)(int) = fi;
	char (*cp)(char) = fc;
	return (1 ? ip : cp) != 0;
}
