struct S { int x; };
typedef struct S S;

int main(void)
{
	struct S s = {1};
	return _Generic(s, struct S: 1, S: 2, default: 3);
}
