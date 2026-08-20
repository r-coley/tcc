struct S {
	int a;
	int b;
	int c;
};

struct S value = { .b = 5, .a = 10, .b = 30 };

int
main(void)
{
	return value.a + value.b + value.c + 2;
}
