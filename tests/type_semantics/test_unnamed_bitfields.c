struct S {
	unsigned int : 0;
	int a;
	unsigned int : 3;
	int b;
};

int
main(void)
{
	struct S s;

	s.a = 19;
	s.b = 23;

	return s.a + s.b;
}
