struct S;
struct S obj;
struct S *p;
struct S { int x; };

int main(void)
{
	struct S *q = p;
	obj.x = 42;
	return q == p ? obj.x : 1;
}
