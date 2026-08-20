struct S {
	int a;
	int b;
};

int main(void)
{
	struct S s = {1, 2, 3};
	return s.a;
}
