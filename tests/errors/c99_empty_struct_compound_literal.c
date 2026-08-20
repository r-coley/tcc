struct S {
	int value;
};

int main(void)
{
	struct S s = (struct S){};
	return s.value;
}
